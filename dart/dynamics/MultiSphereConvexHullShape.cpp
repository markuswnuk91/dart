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

#include "dart/dynamics/MultiSphereConvexHullShape.hpp"

#include "dart/dynamics/BoxShape.hpp"

namespace dart {
namespace dynamics {

//==============================================================================
MultiSphereConvexHullShape::MultiSphereConvexHullShape(const Spheres& spheres)
  : Shape()
{
  addSpheres(spheres);
}

//==============================================================================
MultiSphereConvexHullShape::~MultiSphereConvexHullShape()
{
  // Do nothing
}

//==============================================================================
const std::string& MultiSphereConvexHullShape::getType() const
{
  return getStaticType();
}

//==============================================================================
const std::string& MultiSphereConvexHullShape::getStaticType()
{
  static const std::string type("MultiSphereConvexHullShape");
  return type;
}

//==============================================================================
void MultiSphereConvexHullShape::addSpheres(
    const MultiSphereConvexHullShape::Spheres& spheres)
{
  mSpheres.insert(mSpheres.end(), spheres.begin(), spheres.end());

  mIsBoundingBoxDirty = true;
  mIsVolumeDirty = true;

  incrementVersion();
}

//==============================================================================
void MultiSphereConvexHullShape::addSphere(
    const MultiSphereConvexHullShape::Sphere& sphere)
{
  mSpheres.push_back(sphere);

  mIsBoundingBoxDirty = true;
  mIsVolumeDirty = true;

  incrementVersion();
}

//==============================================================================
void MultiSphereConvexHullShape::addSphere(
    double radius, const math::Vector3d& position)
{
  addSphere(std::make_pair(radius, position));
}

//==============================================================================
void MultiSphereConvexHullShape::removeAllSpheres()
{
  mSpheres.clear();

  mIsBoundingBoxDirty = true;
  mIsVolumeDirty = true;

  incrementVersion();
}

//==============================================================================
std::size_t MultiSphereConvexHullShape::getNumSpheres() const
{
  return mSpheres.size();
}

//==============================================================================
const MultiSphereConvexHullShape::Spheres&
MultiSphereConvexHullShape::getSpheres() const
{
  return mSpheres;
}

//==============================================================================
math::Matrix3d MultiSphereConvexHullShape::computeInertia(double mass) const
{
  // Use bounding box to represent the mesh
  return BoxShape::computeInertia(getBoundingBox().computeFullExtents(), mass);
}

//==============================================================================
ShapePtr MultiSphereConvexHullShape::clone() const
{
  return std::make_shared<MultiSphereConvexHullShape>(mSpheres);
}

//==============================================================================
void MultiSphereConvexHullShape::updateBoundingBox() const
{
  math::Vector3d min = math::Vector3d::Constant(math::max<double>());
  math::Vector3d max = -min;

  for (const auto& sphere : mSpheres) {
    const auto& radius = sphere.first;
    const math::Vector3d& pos = sphere.second;
    const math::Vector3d extent = math::Vector3d::Constant(radius);

    min = min.cwiseMin(pos - extent);
    max = max.cwiseMax(pos + extent);
  }

  mBoundingBox.setMin(min);
  mBoundingBox.setMax(max);

  mIsBoundingBoxDirty = false;
}

//==============================================================================
void MultiSphereConvexHullShape::updateVolume() const
{
  mVolume = BoxShape::computeVolume(mBoundingBox.computeFullExtents());
  mIsVolumeDirty = false;
}

} // namespace dynamics
} // namespace dart
