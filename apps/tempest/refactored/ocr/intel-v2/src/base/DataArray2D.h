///////////////////////////////////////////////////////////////////////////////
///
///	\file    DataArray2D.h
///	\author  Paul Ullrich
///	\version June 26, 2015
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

#ifndef _DATAARRAY2D_H_
#define _DATAARRAY2D_H_

///////////////////////////////////////////////////////////////////////////////

#include "Exception.h"
#include "DataChunk.h"
#include "DataType.h"
#include "DataLocation.h"
#include "Subscript.h"

#include <cstdlib>
#include <cstring>

template <typename T>
class DataArray2D : public DataChunk {

public:
	typedef T ValueType;

	///	<summary>
	///		Constructor.
	///	</summary>
	DataArray2D(
		DataType eDataType = DataType_Default,
		DataLocation eDataLocation = DataLocation_Default
	) :
		m_fOwnsData(true),
		m_eDataType(eDataType),
		m_eDataLocation(eDataLocation),
		m_data1D(NULL)
	{
		m_sSize[0] = 0;
		m_sSize[1] = 0;
	}

	///	<summary>
	///		Constructor allowing specification of size.
	///	</summary>
	DataArray2D(
		size_t sSize0,
		size_t sSize1,
		DataType eDataType = DataType_Default,
		DataLocation eDataLocation = DataLocation_Default,
		bool fAllocate = true
	) :
		m_fOwnsData(true),
		m_eDataType(eDataType),
		m_eDataLocation(eDataLocation),
		m_data1D(NULL)
	{
		m_sSize[0] = sSize0;
		m_sSize[1] = sSize1;

		if (fAllocate) {
			Allocate();
		}
	}

	///	<summary>
	///		Copy constructor.
	///	</summary>
	DataArray2D(const DataArray2D<T> & da) :
		m_fOwnsData(true),
		m_eDataType(DataType_Default),
		m_eDataLocation(DataLocation_Default)
	{
		m_sSize[0] = 0;
		m_sSize[1] = 0;

		Assign(da);
	}

	///	<summary>
	///		Destructor.
	///	</summary>
	virtual ~DataArray2D() {
		Detach();
	}

	///	<summary>
	///		Get the size of this DataChunk, in bytes.
	///	</summary>
	virtual size_t GetByteSize() const {
		size_t sSize = (m_sSize[0] * m_sSize[1] * sizeof(T));

		if (sSize % sizeof(size_t) == 0) {
			return sSize;
		} else {
			return (sSize / sizeof(size_t) + 1) * sizeof(size_t);
		}
	}

	///	<summary>
	///		Allocate data in this DataArray2D.
	///	</summary>
	void Allocate(
		size_t sSize0 = 0,
		size_t sSize1 = 0
	) {
		if (!m_fOwnsData) {
			_EXCEPTIONT("Attempting to Allocate() on attached DataArray2D");
		}

		if (sSize0 == 0) {
			sSize0 = m_sSize[0];
		}
		if (sSize1 == 0) {
			sSize1 = m_sSize[1];
		}
		if ((sSize0 == 0) || (sSize1 == 0)) {
			_EXCEPTIONT("Attempting to Allocate() zero-size DataArray2D");
		}
		if ((m_data1D == NULL) ||
		    (m_sSize[0] != sSize0) ||
		    (m_sSize[1] != sSize1)
		) {
			Detach();

			m_sSize[0] = sSize0;
			m_sSize[1] = sSize1;

			m_data1D = reinterpret_cast<T *>(malloc(GetByteSize()));

		}

		Zero();

		m_fOwnsData = true;
	}

	///	<summary>
	///		Set the dimension sizes of this DataArray2D.
	///	</summary>
	inline void SetSize(
		size_t sSize0,
		size_t sSize1
	) {
		if (IsAttached()) {
			_EXCEPTIONT("Attempting SetSize() on attached DataArray2D");
		}

		m_sSize[0] = sSize0;
		m_sSize[1] = sSize1;
	}

public:
	///	<summary>
	///		Determine if this DataChunk is attached to a data array.
	///	</summary>
	virtual bool IsAttached() const {
		return (m_data1D != NULL);
	}

	///	<summary>
	///		Attach this DataChunk to an array of pre-allocated data.
	///	</summary>
	virtual void AttachToData(void * ptr) {
		if (IsAttached()) {
			_EXCEPTIONT("Attempting AttachToData() on attached DataArray2D");
		}

		m_data1D = reinterpret_cast<T *>(ptr);
		m_fOwnsData = false;
	}

	///	<summary>
	///		Detach data from this DataChunk.
	///	</summary>
	virtual void Detach() {
		if ((m_fOwnsData) && (m_data1D != NULL)) {
			delete[] m_data1D;
		}
		m_fOwnsData = true;
		m_data1D = NULL;
	}

	///	<summary>
	///		Deallocate data from this DataChunk.
	///	</summary>
	void Deallocate() {
		if (!m_fOwnsData) {
			_EXCEPTIONT("Attempting Deallocate() on attached DataArray2D");
		}

		Detach();
	}

public:
	///	<summary>
	///		Get the size of the data.
	///	</summary>
	size_t GetTotalSize() const {
		return (m_sSize[0] * m_sSize[1]);
	}

	///	<summary>
	///		Get the number of rows in this DataArray2D.
	///	</summary>
	inline size_t GetRows() const {
		return m_sSize[0];
	}

	///	<summary>
	///		Get the number of columns in this DataArray2D.
	///	</summary>
	inline size_t GetColumns() const {
		return m_sSize[1];
	}

public:
	///	<summary>
	///		Set the DataLocation.
	///	</summary>
	inline void SetDataLocation(const DataLocation & eDataLocation) {
		m_eDataLocation = eDataLocation;
	}

	///	<summary>
	///		Set the DataType.
	///	</summary>
	inline void SetDataType(const DataType & eDataType) {
		m_eDataType = eDataType;
	}

	///	<summary>
	///		Get the DataLocation.
	///	</summary>
	inline DataLocation GetDataLocation() const {
		return m_eDataLocation;
	}

	///	<summary>
	///		Get the DataType.
	///	</summary>
	inline DataType GetDataType() const {
		return m_eDataType;
	}

public:
	///	<summary>
	///		Assignment operator.
	///	</summary>
	void Assign(const DataArray2D<T> & da) {

		// Verify source array existence
		if (!da.IsAttached()) {
			if (!IsAttached()) {
				m_sSize[0] = da.m_sSize[0];
				m_sSize[1] = da.m_sSize[1];

				m_eDataType = da.m_eDataType;
				m_eDataLocation = da.m_eDataLocation;
				return;
			}

			_EXCEPTIONT("Attempting to assign unattached DataArray2D "
				"to attached DataArray2D (undefined behavior)");
		}

		// Allocate if necessary
		if (!IsAttached()) {
			Allocate(da.m_sSize[0], da.m_sSize[1]);
			m_eDataType = da.m_eDataType;
			m_eDataLocation = da.m_eDataLocation;
		}
		if (IsAttached() && m_fOwnsData) {
			if ((m_sSize[0] != da.m_sSize[0]) ||
			    (m_sSize[1] != da.m_sSize[1])
			) {
				Deallocate();
				Allocate(da.m_sSize[0], da.m_sSize[1]);
			}

			Deallocate();
			Allocate(da.m_sSize[0], da.m_sSize[1]);
			m_eDataType = da.m_eDataType;
			m_eDataLocation = da.m_eDataLocation;
		}

		// Check initialization status
		if (da.GetRows() != GetRows()) {
			_EXCEPTIONT("Rows mismatch in assignment of DataArray2D");
		}
		if (da.GetColumns() != GetColumns()) {
			_EXCEPTIONT("Columns mismatch in assignment of DataArray2D");
		}
		if (da.m_eDataType != m_eDataType) {
			_EXCEPTIONT("DataType mismatch in assignment of DataArray2D");
		}
		if (da.m_eDataLocation != m_eDataLocation) {
			_EXCEPTIONT("DataLocation mismatch in assignment of DataArray2D");
		}

		// Copy data
		memcpy(m_data1D, da.m_data1D, GetByteSize());
	}

	///	<summary>
	///		Assignment operator.
	///	</summary>
	DataArray2D<T> & operator= (const DataArray2D<T> & da) {
		Assign(da);
		return (*this);
	}

public:
	///	<summary>
	///		Zero the data content of this object.
	///	</summary>
	void Zero() {

		// Check that this DataArray2D is attached to a data object
		if (!IsAttached()) {
			_EXCEPTIONT("Attempted operation on unattached DataArray2D");
		}

		// Set content to zero
		memset(m_data1D, 0, GetByteSize());
	}

	///	<summary>
	///		Scale data by a given constant.
	///	</summary>
	void Scale(const T & x) {

		// Check that this DataArray2D is attached to a data object
		if (!IsAttached()) {
			_EXCEPTIONT("Attempted operation on unattached DataArray2D");
		}

		// Scale data values
		size_t sTotalSize = GetTotalSize();

		for (size_t i = 0; i < sTotalSize; i++) {
			m_data1D[i] *= x;
		}
	}

	///	<summary>
	///		Add a factor of the given DataArray2D to this DataArray2D.
	///	</summary>
	void AddProduct(
		const DataArray2D<T> & da,
		const T & x
	) {
		// Check that this DataArray2D is attached to a data object
		if (!IsAttached()) {
			_EXCEPTIONT("Attempted operation on unattached DataArray2D");
		}
		if (!da.IsAttached()) {
			_EXCEPTIONT("Attempted operation on unattached DataArray2D");
		}
		if (da.GetRows() != GetRows()) {
			_EXCEPTIONT("Rows mismatch in DataArray2D");
		}
		if (da.GetColumns() != GetColumns()) {
			_EXCEPTIONT("Columns mismatch in DataArray2D");
		}

		// Scale data values
		size_t sTotalSize = GetTotalSize();

		for (size_t i = 0; i < sTotalSize; i++) {
			m_data1D[i] += x * da.m_data1D[i];
		}
	}

public:
	///	<summary>
	///		Subscript DSEL operator.
	///	</summary>
	inline Subscript<DataArray2D<T> const, 1, 2>
	operator[](std::ptrdiff_t idx) const noexcept
	{
		Subscript<DataArray2D<T> const, 2, 2> s(*this);
		return s[idx];
	}
	///	<summary>
	///		Subscript DSEL operator.
	///	</summary>
	inline Subscript<DataArray2D<T>, 1, 2>
	operator[](std::ptrdiff_t idx) noexcept
	{
		Subscript<DataArray2D<T>, 2, 2> s(*this);
		return s[idx];
	}

	///	<summary>
	///		Parenthetical array accessor.
	///	</summary>
	inline T const&
	operator()(std::array<std::ptrdiff_t, 2> indices) const noexcept
	{
		return (*this)(indices[0], indices[1]);
	}
	///	<summary>
	///		Parenthetical array accessor.
	///	</summary>
	inline T&
	operator()(std::array<std::ptrdiff_t, 2> indices) noexcept
	{
		return (*this)(indices[0], indices[1]);
	}

	///	<summary>
	///		Parenthetical array accessor.
	///	</summary>
	inline const T & operator()(size_t i, size_t j) const noexcept {
		return (*(m_data1D + i * m_sSize[1] + j));
	}
	///	<summary>
	///		Parenthetical array accessor.
	///	</summary>
	inline T & operator()(size_t i, size_t j) noexcept {
		return (*(m_data1D + i * m_sSize[1] + j));
	}

	///	<summary>
	///		Parenthetical array accessor (unit-stride slicer).
	///	</summary>
	inline T const*
	operator()(std::array<std::ptrdiff_t, 1> indices) const noexcept
	{
		return (*this)(indices[0]);
	}
	///	<summary>
	///		Parenthetical array accessor (unit-stride slicer).
	///	</summary>
	inline T*
	operator()(std::array<std::ptrdiff_t, 1> indices) noexcept
	{
		return (*this)(indices[0]);
	}

	///	<summary>
	///		Parenthetical array accessor (unit-stride slicer).
	///	</summary>
	inline T const* operator()(size_t i) const noexcept {
		return m_data1D + i * m_sSize[1];
	}
	///	<summary>
	///		Parenthetical array accessor (unit-stride slicer).
	///	</summary>
	inline T* operator()(size_t i) noexcept {
		return m_data1D + i * m_sSize[1];
	}

private:
	///	<summary>
	///		A flag indicating this array owns its data.
	///	</summary>
	bool m_fOwnsData;

	///	<summary>
	///		The size of each dimension of this DataArray3D.
	///	</summary>
	size_t m_sSize[2];

	///	<summary>
	///		The type of data stored in this DataArray2D.
	///	</summary>
	DataType m_eDataType;

	///	<summary>
	///		The location of data stored in this DataArray2D.
	///	</summary>
	DataLocation m_eDataLocation;

	///	<summary>
	///		A pointer to the data for this DataArray2D.
	///	</summary>
	T * m_data1D;
};

///////////////////////////////////////////////////////////////////////////////

#endif

