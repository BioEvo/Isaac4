################################################################################
##
## Isaac Genome Alignment Software
## Copyright (c) 2010-2017 Illumina, Inc.
## All rights reserved.
##
## This software is provided under the terms and conditions of the
## GNU GENERAL PUBLIC LICENSE Version 3
##
## You should have received a copy of the GNU GENERAL PUBLIC LICENSE Version 3
## along with this program. If not, see
## <https://github.com/illumina/licenses/>.
##
################################################################################
##
## file CMakeLists.txt
##
## Configuration file for the COPYRIGHT and Changes file installation
##
## author Roman Petrovski
##
################################################################################

include ("${iSAAC_GLOBALS_CMAKE}")

install(CODE "
    file(INSTALL DESTINATION \"${iSAAC_ORIG_DATADIR}\" TYPE FILE FILES \"${Isaac_SOURCE_DIR}/Changes\" \"${CMAKE_SOURCE_DIR}/../COPYRIGHT\" PERMISSIONS ${iSAAC_LIBRARY_PERMISSIONS})
    ")
