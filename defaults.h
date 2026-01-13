/*

 */

#ifndef __PC_CLIENT_DEFAULTS_H_
#define __PC_CLIENT_DEFAULTS_H_

#include <stdint.h>

#include <string>

extern const char kAudioLabel[];
extern const char kVideoLabel[];
extern const char kStreamId[];
extern const uint16_t kDefaultServerPort;

std::string GetEnvVarOrDefault(const char* env_var_name,
                               const char* default_value);
std::string GetPeerConnectionString();
std::string GetDefaultServerName();
std::string GetPeerName();

#endif  // __PC_CLIENT_DEFAULTS_H_
