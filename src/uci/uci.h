#pragma once

#include <iosfwd>

namespace Eunshin::UCI {

// Runs one UCI protocol session.  Supplying streams keeps the production
// frontend usable by deterministic protocol tests without a second parser.
int run(std::istream& input, std::ostream& output);

} // namespace Eunshin::UCI
