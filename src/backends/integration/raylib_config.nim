## Raylib configuration for Kryon
## This file must be imported BEFORE raylib to set compile flags

# Enable all common image format support
{.passC: "-DSUPPORT_FILEFORMAT_PNG".}
{.passC: "-DSUPPORT_FILEFORMAT_JPG".}
{.passC: "-DSUPPORT_FILEFORMAT_BMP".}
{.passC: "-DSUPPORT_FILEFORMAT_TGA".}
{.passC: "-DSUPPORT_FILEFORMAT_GIF".}
{.passC: "-DSUPPORT_FILEFORMAT_QOI".}
{.passC: "-DSUPPORT_FILEFORMAT_DDS".}
