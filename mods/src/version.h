#pragma once

// clang-format off
#define VERSION_MAJOR               0
#define VERSION_MINOR               6
#define VERSION_REVISION            1
#define VERSION_PATCH               8

#define STRINGIFY_(s)               #s
#define STRINGIFY(s)                STRINGIFY_(s)

#if VERSION_PATCH
	#define VERSION_PATCH_STR " (Beta)"
#else
	#define VERSION_PATCH_STR ""
#endif

#define VER_FILE_DESCRIPTION_STR    "Star Trek Fleet Command: Community Mod" VERSION_PATCH_STR

#define VER_FILE_VERSION            VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION, VERSION_PATCH
#define VER_FILE_VERSION_STR        STRINGIFY(VERSION_MAJOR) "." STRINGIFY(VERSION_MINOR) "." STRINGIFY(VERSION_REVISION) "." STRINGIFY(VERSION_PATCH)

#define VER_PRODUCTNAME_STR         "STFC: Community Mod"
#define VER_PRODUCT_VERSION         VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION
#define VER_PRODUCT_VERSION_STR     VER_FILE_VERSION_STR
#define VER_ORIGINAL_FILENAME_STR   "stfc-community-patch.dll"
#define VER_INTERNAL_NAME_STR       VER_ORIGINAL_FILENAME_STR
#define VER_COPYRIGHT_STR           "Copyright (C) 2025"

#ifdef DEBUG
#define VER_FILEFLAGS               VS_FF_DEBUG
#else
#define VER_FILEFLAGS               0
#endif

#ifndef VS_VERSION_INFO
#define VS_VERSION_INFO 1
#endif
// clang-format on
