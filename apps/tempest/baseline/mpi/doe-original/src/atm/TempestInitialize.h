///////////////////////////////////////////////////////////////////////////////
///
///	\file    TempestInitialize.h
///	\author  Paul Ullrich
///	\version June 4, 2014
///
///	<remarks>
///		Copyright 2000-2010 Paul Ullrich
///
///		This file is distributed as part of the Tempest source code package.
///		Permission is granted to use, copy, modify and distribute this
///		source code and its documentation under the terms of the GNU General
///		Public License.  This software is provided "as is" without express
///		or implied warranty.
///	</remarks>

#ifndef _TEMPESTINITIALIZE_H_
#define _TEMPESTINITIALIZE_H_

#include "Defines.h"
#include "Model.h"
#include "TimestepSchemeStrang.h"
#include "TimestepSchemeARK2.h"
#include "TimestepSchemeARK3.h"
#include "HorizontalDynamicsFEM.h"
#include "HorizontalDynamicsDG.h"
#include "VerticalDynamicsStub.h"
#include "VerticalDynamicsFEM.h"
#include "OutputManagerComposite.h"
#include "OutputManagerReference.h"
#include "OutputManagerChecksum.h"
#include "GridCSGLL.h"
#include "GridCartesianGLL.h"
#include "VerticalStretch.h"

#include "TimeObj.h"
#include "Announce.h"
#include "CommandLine.h"
#include "STLStringHelper.h"
#include <string>
#include "mpi.h"

#ifdef USE_PETSC
#include <petscsnes.h>
#endif

///////////////////////////////////////////////////////////////////////////////

struct _TempestCommandLineVariables {
	ModelParameters param;
	bool fNoOutput;
	std::string strOutputDir;
	std::string strOutputPrefix;
	int nOutputsPerFile;
	Time timeOutputDeltaT;
	Time timeOutputRestartDeltaT;
	int nOutputResX;
	int nOutputResY;
	bool fOutputVorticity;
	bool fOutputDivergence;
	bool fOutputTemperature;
	bool fNoReferenceState;
	bool fNoTracers;
	bool fNoHyperviscosity;
	double dNuScalar;
	double dNuDiv;
	double dNuVort;
	bool fExplicitVertical;
	std::string strVerticalStaggering;
	std::string strVerticalStretch;
	int nVerticalHyperdiffOrder;
	std::string strTimestepScheme;
	std::string strHorizontalDynamics;
	int nResolutionX;
	int nResolutionY;
	int nLevels;
	int nHorizontalOrder;
	int nVerticalOrder;
};

///////////////////////////////////////////////////////////////////////////////

#define _TempestDefineCommandLineDefault(TestCaseName) \
	CommandLineBool(_tempestvars.fNoOutput, "output_none"); \
	CommandLineString(_tempestvars.strOutputDir, "output_dir", "out" TestCaseName); \
	CommandLineString(_tempestvars.strOutputPrefix, "output_prefix", "out"); \
	CommandLineString(_tempestvars.param.m_strRestartFile, "restart_file", ""); \
	CommandLineInt(_tempestvars.nOutputsPerFile, "output_perfile", -1); \
	CommandLineDeltaTime(_tempestvars.timeOutputRestartDeltaT, "output_restart_dt", ""); \
	CommandLineInt(_tempestvars.nOutputResX, "output_x", 360); \
	CommandLineInt(_tempestvars.nOutputResY, "output_y", 180); \
	CommandLineBool(_tempestvars.fOutputVorticity, "output_vort"); \
	CommandLineBool(_tempestvars.fOutputDivergence, "output_div"); \
	CommandLineBool(_tempestvars.fOutputTemperature, "output_temp"); \
	CommandLineBool(_tempestvars.fNoReferenceState, "norefstate"); \
	CommandLineBool(_tempestvars.fNoTracers, "notracers"); \
	CommandLineBool(_tempestvars.fNoHyperviscosity, "nohypervis"); \
	CommandLineDouble(_tempestvars.dNuScalar, "nu", 1.0e15); \
	CommandLineDouble(_tempestvars.dNuDiv, "nud", 1.0e15); \
	CommandLineDouble(_tempestvars.dNuVort, "nuv", 1.0e15); \
	CommandLineBool(_tempestvars.fExplicitVertical, "explicitvertical"); \
	CommandLineStringD(_tempestvars.strVerticalStaggering, "vstagger", "CPH", "(LEV | INT | LOR | CPH)"); \
	CommandLineString(_tempestvars.strVerticalStretch, "vstretch", "uniform"); \
	CommandLineInt(_tempestvars.nVerticalHyperdiffOrder, "verthypervisorder", 0); \
	CommandLineString(_tempestvars.strTimestepScheme, "timescheme", "strang"); \
	CommandLineStringD(_tempestvars.strHorizontalDynamics, "method", "SE", "(SE | DG)");

///////////////////////////////////////////////////////////////////////////////

#define BeginTempestCommandLine(TestCaseName) \
	_TempestCommandLineVariables _tempestvars; \
	BeginCommandLine() \
	_TempestDefineCommandLineDefault(TestCaseName)

#define EndTempestCommandLine(argv) \
	EndCommandLine(argv)

#define SetDefaultResolution(default_resolution) \
	CommandLineInt(_tempestvars.nResolutionX, "resolution", default_resolution);

#define SetDefaultResolutionX(default_resolution_x) \
	CommandLineInt(_tempestvars.nResolutionX, "resx", default_resolution_x);

#define SetDefaultResolutionY(default_resolution_y) \
	CommandLineInt(_tempestvars.nResolutionY, "resy", default_resolution_y);

#define SetDefaultLevels(default_levels) \
	CommandLineInt(_tempestvars.nLevels, "levels", default_levels);

#define SetDefaultOutputDeltaT(default_output_dt) \
	CommandLineDeltaTime(_tempestvars.timeOutputDeltaT, "outputtime", default_output_dt);

#define SetDefaultDeltaT(default_dt) \
	CommandLineDeltaTime(_tempestvars.param.m_timeDeltaT, "dt", default_dt);

#define SetDefaultEndTime(default_endtime) \
	CommandLineFixedTime(_tempestvars.param.m_timeEnd, "endtime", default_endtime);

#define SetDefaultHorizontalOrder(default_order) \
	CommandLineInt(_tempestvars.nHorizontalOrder, "order", default_order)

#define SetDefaultVerticalOrder(default_vertorder) \
	CommandLineInt(_tempestvars.nVerticalOrder, "vertorder", default_vertorder)

///////////////////////////////////////////////////////////////////////////////

void _TempestSetupMethodOfLines(
	Model & model,
	_TempestCommandLineVariables & vars
) {
	// Set the timestep scheme
	AnnounceStartBlock("Initializing time scheme");

	STLStringHelper::ToLower(vars.strTimestepScheme);
	if (vars.strTimestepScheme == "strang") {
		model.SetTimestepScheme(
			new TimestepSchemeStrang(model));

	} else if (vars.strTimestepScheme == "strang/rk4") {
		model.SetTimestepScheme(
			new TimestepSchemeStrang(
				model, 0.0, TimestepSchemeStrang::RungeKutta4));

	} else if (vars.strTimestepScheme == "strang/rk3") {
		model.SetTimestepScheme(
			new TimestepSchemeStrang(
				model, 0.0, TimestepSchemeStrang::RungeKuttaSSP3));

	} else if (vars.strTimestepScheme == "strang/kgu35") {
		model.SetTimestepScheme(
			new TimestepSchemeStrang(
				model, 0.0, TimestepSchemeStrang::KinnmarkGrayUllrich35));

	} else if (vars.strTimestepScheme == "strang/ssprk53") {
		model.SetTimestepScheme(
			new TimestepSchemeStrang(
				model, 0.0, TimestepSchemeStrang::RungeKuttaSSPRK53));

	} else if (vars.strTimestepScheme == "ark2") {
		model.SetTimestepScheme(
			new TimestepSchemeARK2(model));

	} else if (vars.strTimestepScheme == "ark3") {
		model.SetTimestepScheme(
			new TimestepSchemeARK3(model));

	} else {
		_EXCEPTIONT("Invalid timescheme: Expected "
			"\"Strang\", \"ARK2\", \"ARK3\"");
	}
	AnnounceEndBlock("Done");

	// Set the horizontal dynamics
	AnnounceStartBlock("Initializing horizontal dynamics");

	if (vars.fNoHyperviscosity) {
		vars.dNuScalar = 0.0;
		vars.dNuDiv = 0.0;
		vars.dNuVort = 0.0;
	}

	STLStringHelper::ToLower(vars.strHorizontalDynamics);
	if (vars.strHorizontalDynamics == "se") {
		model.SetHorizontalDynamics(
			new HorizontalDynamicsFEM(
				model,
				vars.nHorizontalOrder,
				vars.dNuScalar,
				vars.dNuDiv,
				vars.dNuVort));

	} else if (vars.strHorizontalDynamics == "dg") {
		model.SetHorizontalDynamics(
			new HorizontalDynamicsDG(
				model,
				vars.nHorizontalOrder,
				vars.dNuScalar,
				vars.dNuDiv,
				vars.dNuVort));

	} else {
		_EXCEPTIONT("Invalid method: Expected \"SE\" or \"DG\"");
	}
	AnnounceEndBlock("Done");

	// Vertical staggering
	STLStringHelper::ToLower(vars.strVerticalStaggering);

	// Set the vertical dynamics
	AnnounceStartBlock("Initializing vertical dynamics");
	if (vars.nLevels == 1) {
		model.SetVerticalDynamics(
			new VerticalDynamicsStub(model));

	} else if (vars.strVerticalStaggering == "int") {
		model.SetVerticalDynamics(
			new VerticalDynamicsFEM(
				model,
				vars.nHorizontalOrder,
				vars.nVerticalOrder,
				vars.nVerticalHyperdiffOrder,
				vars.fExplicitVertical,
				!vars.fNoReferenceState,
				true,
				true));

	} else {
		model.SetVerticalDynamics(
			new VerticalDynamicsFEM(
				model,
				vars.nHorizontalOrder,
				vars.nVerticalOrder,
				vars.nVerticalHyperdiffOrder,
				vars.fExplicitVertical,
				!vars.fNoReferenceState));
	}

	AnnounceEndBlock("Done");
}

///////////////////////////////////////////////////////////////////////////////

void _TempestSetupOutputManagers(
	Model & model,
	_TempestCommandLineVariables & vars
) {
	// Set the reference output manager for the model
	if (!vars.fNoOutput) {
		AnnounceStartBlock("Creating reference output manager");
		OutputManagerReference * pOutmanRef =
			new OutputManagerReference(
				*(model.GetGrid()),
				vars.timeOutputDeltaT,
				vars.strOutputDir,
				vars.strOutputPrefix,
				vars.nOutputsPerFile,
				vars.nOutputResX,
				vars.nOutputResY,
				false,
				true);

		if (vars.fOutputVorticity) {
			pOutmanRef->OutputVorticity();
		}
		if (vars.fOutputDivergence) {
			pOutmanRef->OutputDivergence();
		}
		if (vars.fOutputTemperature) {
			pOutmanRef->OutputTemperature();
		}

		model.AttachOutputManager(pOutmanRef);
		AnnounceEndBlock("Done");
	}

	// Set the composite output manager for the model
	if ((! vars.timeOutputRestartDeltaT.IsZero()) ||
		(vars.param.m_strRestartFile != "")
	) {
		AnnounceStartBlock("Creating composite output manager");
		model.AttachOutputManager(
			new OutputManagerComposite(
				*(model.GetGrid()),
				vars.timeOutputRestartDeltaT,
				vars.strOutputDir,
				vars.strOutputPrefix));
		AnnounceEndBlock("Done");
	}

	// Set the checksum output manager for the model
	AnnounceStartBlock("Creating checksum output manager");
	model.AttachOutputManager(
		new OutputManagerChecksum(
			*(model.GetGrid()),
			vars.timeOutputDeltaT));
	AnnounceEndBlock("Done");
}

///////////////////////////////////////////////////////////////////////////////

void _TempestSetupCubedSphereModel(
	Model & model,
	_TempestCommandLineVariables & vars
) {
	// Set the parameters
	model.SetParameters(vars.param);

	// Setup Method of Lines
	_TempestSetupMethodOfLines(model, vars);

	// Get the vertical staggering from --vstagger
	Grid::VerticalStaggering eVerticalStaggering;
	STLStringHelper::ToLower(vars.strVerticalStaggering);
	if (vars.strVerticalStaggering == "lev") {
		eVerticalStaggering = Grid::VerticalStaggering_Levels;

	} else if (vars.strVerticalStaggering == "int") {
		eVerticalStaggering = Grid::VerticalStaggering_Interfaces;

	} else if (vars.strVerticalStaggering == "lor") {
		eVerticalStaggering = Grid::VerticalStaggering_Lorenz;

	} else if (vars.strVerticalStaggering == "cph") {
		eVerticalStaggering = Grid::VerticalStaggering_CharneyPhillips;

	} else {
		_EXCEPTIONT("Invalid value for --vstagger");
	}

	// Construct the Grid
	AnnounceStartBlock("Constructing grid");

	Grid * pGrid =
		new GridCSGLL(
			model,
			vars.nResolutionX,
			4,
			vars.nHorizontalOrder,
			vars.nVerticalOrder,
			vars.nLevels,
			eVerticalStaggering);

	// Set the vertical stretching function
	STLStringHelper::ToLower(vars.strVerticalStretch);
	if (vars.strVerticalStretch == "uniform") {

	} else if (vars.strVerticalStretch == "cubic") {
		pGrid->SetVerticalStretchFunction(
			new VerticalStretchCubic);

	} else if (vars.strVerticalStretch == "pwlinear") {
		pGrid->SetVerticalStretchFunction(
			new VerticalStretchPiecewiseLinear);

	} else {
		_EXCEPTIONT("Invalid value for --vstretch");
	}

	// Set the Model Grid
	model.SetGrid(pGrid);

	AnnounceEndBlock("Done");

	// Setup OutputManagers
	_TempestSetupOutputManagers(model, vars);
}

///////////////////////////////////////////////////////////////////////////////

void _TempestSetupCartesianModel(
	Model & model,
	double dGDim[],
	double dRefLat,
	_TempestCommandLineVariables & vars
) {
	// Set the parameters
	model.SetParameters(vars.param);

	// Setup Method of Lines
	_TempestSetupMethodOfLines(model, vars);

	// Get the vertical staggering from --vstagger
	Grid::VerticalStaggering eVerticalStaggering;
	STLStringHelper::ToLower(vars.strVerticalStaggering);
	if (vars.strVerticalStaggering == "lev") {
		eVerticalStaggering = Grid::VerticalStaggering_Levels;

	} else if (vars.strVerticalStaggering == "int") {
		eVerticalStaggering = Grid::VerticalStaggering_Interfaces;

	} else if (vars.strVerticalStaggering == "lor") {
		eVerticalStaggering = Grid::VerticalStaggering_Lorenz;

	} else if (vars.strVerticalStaggering == "cph") {
		eVerticalStaggering = Grid::VerticalStaggering_CharneyPhillips;

	} else {
		_EXCEPTIONT("Invalid value for --vstagger");
	}

	// Set the model grid
	AnnounceStartBlock("Constructing grid");

	Grid * pGrid =
		new GridCartesianGLL(
			model,
			vars.nResolutionX,
			vars.nResolutionY,
			4,
			vars.nHorizontalOrder,
			vars.nVerticalOrder,
			vars.nLevels,
			dGDim,
			dRefLat,
			eVerticalStaggering);

	// Set the vertical stretching function
	STLStringHelper::ToLower(vars.strHorizontalDynamics);
	if (vars.strVerticalStretch == "uniform") {

	} else if (vars.strVerticalStretch == "cubic") {
		pGrid->SetVerticalStretchFunction(
			new VerticalStretchCubic);

	} else if (vars.strVerticalStretch == "pwlinear") {
		pGrid->SetVerticalStretchFunction(
			new VerticalStretchPiecewiseLinear);

	} else {
		_EXCEPTIONT("Invalid value for --vstretch");

	}

	// Set the Model Grid
	model.SetGrid(pGrid);

	AnnounceEndBlock("Done");

	// Setup OutputManagers
	_TempestSetupOutputManagers(model, vars);
}

///////////////////////////////////////////////////////////////////////////////

#define TempestSetupCubedSphereModel(model) \
	_TempestSetupCubedSphereModel(model, _tempestvars);

#define TempestSetupCartesianModel(model, dimensions, latitude) \
	_TempestSetupCartesianModel(model, dimensions, latitude, _tempestvars);

///////////////////////////////////////////////////////////////////////////////

void TempestInitialize(int * argc, char*** argv) {

#ifdef USE_PETSC
	// Initialize PetSc
	PetscInitialize(argc, argv, NULL, NULL);
#else
	// Initialize MPI
	MPI_Init(argc, argv);
#endif

}

///////////////////////////////////////////////////////////////////////////////

void TempestDeinitialize() {

#ifdef USE_PETSC
	// Finalize PetSc
	PetscFinalize();
#else
	// Finalize MPI
	MPI_Finalize();
#endif

}

///////////////////////////////////////////////////////////////////////////////

#endif

