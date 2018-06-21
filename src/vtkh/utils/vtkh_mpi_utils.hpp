//---------------------------------------------------------------------------//
// vtkh_mpi_utils.hpp
//---------------------------------------------------------------------------//
// note: this is a private header, only used internal to vtkh to keep
// MPI deps out of the public interface.
//---------------------------------------------------------------------------//
#ifndef VTKH_MPI_UTILS_HPP
#define VTKH_MPI_UTILS_HPP

#include <mpi.h>

//---------------------------------------------------------------------------//
// begin vtkh:: namespace
//---------------------------------------------------------------------------//
namespace vtkh
{

//---------------------------------------------------------------------------//
MPI_Comm GetMPIComm();

//---------------------------------------------------------------------------//
// end vtkh:: namespace
//---------------------------------------------------------------------------//
};
#endif
