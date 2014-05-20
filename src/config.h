/**************************************************************************
* config :
* - parse config file in Panel struct.
*
* Check COPYING file for Copyright
*
**************************************************************************/

#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

extern char* config_path;
extern char* snapshot_path;

// default global data
void default_config(void);

// freed memory
void cleanup_config(void);

// Tries to read a given configuration file.
bool config_read_file(const char* path);

// Tries the default config from instalation path.
bool config_read(void);

#endif
