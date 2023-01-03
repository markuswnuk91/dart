/*
 * Copyright (c) 2011-2023, The DART development contributors
 * All rights reserved.
 *
 * The list of contributors can be found at:
 *   https://github.com/dartsim/dart/blob/master/LICENSE
 *
 * This file is provided under the following "BSD-style" License:
 *   Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the following
 *   conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *   CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *   INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 *   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 *   USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 *   AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *   ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *   POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DART_UTILS_SKELPARSER_HPP_
#define DART_UTILS_SKELPARSER_HPP_

#include "dart/common/LocalResourceRetriever.hpp"
#include "dart/common/Uri.hpp"
#include "dart/simulation/World.hpp"

#include <string>

namespace dart {
namespace io {

/// SkelParser
namespace SkelParser {

/// Read World from skel file
simulation::WorldPtr readWorld(
    const common::Uri& uri,
    const common::ResourceRetrieverPtr& retriever = nullptr);

/// Read World from an xml-formatted string
simulation::WorldPtr readWorldXML(
    const std::string& xmlString,
    const common::Uri& baseUri = "",
    const common::ResourceRetrieverPtr& retriever = nullptr);

/// Read Skeleton from skel file
dynamics::SkeletonPtr readSkeleton(
    const common::Uri& uri,
    const common::ResourceRetrieverPtr& retriever = nullptr);

} // namespace SkelParser

} // namespace io
} // namespace dart

#endif // #ifndef DART_UTILS_SKELPARSER_HPP_