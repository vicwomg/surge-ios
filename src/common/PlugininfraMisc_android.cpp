/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2026, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 */

#include <sst/plugininfra/misc_platform.h>

#include <cerrno>
#include <cstring>

namespace sst
{
namespace plugininfra
{
namespace misc_platform
{
bool isDarkMode() { return true; }

void allocateConsole() {}

std::string toOSCase(const std::string &text) { return text; }

std::string stackTraceToString(int) { return "Stack Trace not available on Android"; }

std::string getLastSystemError() { return std::string(std::strerror(errno)); }
} // namespace misc_platform
} // namespace plugininfra
} // namespace sst
