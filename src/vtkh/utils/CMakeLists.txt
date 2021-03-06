#==============================================================================
# See License.txt
#==============================================================================
set(vtkh_utils_headers
  PNGEncoder.hpp
  vtkm_array_utils.hpp
  vtkm_dataset_info.hpp
  )

set(vtkh_utils_sources
  PNGEncoder.cpp
  vtkm_dataset_info.cpp
  )

set(vtkh_utils_thirdparty_libs vtkm vtkh_lodepng)

#if(ENABLE_OPENMP)
#  list(APPEND vtkh_utils_thirdparty_libs openmp)
#endif()

if(CUDA_FOUND)
  list(APPEND vtkh_utils_thirdparty_libs cuda)
endif()

blt_add_library(
  NAME vtkh_utils
  SOURCES ${vtkh_utils_sources}
  HEADERS ${vtkh_utils_headers}
  DEPENDS_ON ${vtkh_utils_thirdparty_libs}
  )

if(ENABLE_OPENMP)
  if(CUDA_FOUND) 
    blt_add_target_compile_flags(TO vtkh_utils FLAGS "-Xcompiler ${OpenMP_CXX_FLAGS}")
  else()
    blt_add_target_compile_flags(TO vtkh_utils FLAGS "${OpenMP_CXX_FLAGS}")
  endif()
endif()



# Install libraries
install(TARGETS vtkh_utils
  EXPORT ${VTKh_EXPORT_NAME}
  ARCHIVE DESTINATION ${VTKh_INSTALL_LIB_DIR}
  LIBRARY DESTINATION ${VTKh_INSTALL_LIB_DIR}
  RUNTIME DESTINATION ${VTKh_INSTALL_BIN_DIR}
  )

# Install headers
install(FILES ${vtkh_utils_headers}
  DESTINATION ${VTKh_INSTALL_INCLUDE_DIR}/vtkh/utils)

if (MPI_FOUND)

    blt_add_library(
      NAME vtkh_utils_mpi
      SOURCES ${vtkh_utils_sources} vtkh_mpi_utils.cpp
      HEADERS ${vtkh_utils_headers} vtkh_mpi_utils.hpp
      DEPENDS_ON ${vtkh_utils_thirdparty_libs} mpi vtkh_core
      )

    # Install libraries
    install(TARGETS vtkh_utils_mpi
      EXPORT ${VTKh_EXPORT_NAME}
      ARCHIVE DESTINATION ${VTKh_INSTALL_LIB_DIR}
      LIBRARY DESTINATION ${VTKh_INSTALL_LIB_DIR}
      RUNTIME DESTINATION ${VTKh_INSTALL_BIN_DIR}
      )
endif()
