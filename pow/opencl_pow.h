// Copyright 2018 The Beam Team
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once
//#include <boost/dll.hpp>
#include <vector>
#include <string>

namespace beam
{
    struct GpuInfo
    {
        std::string name;
        size_t index;
    };

    //class IExternalPOW;

    //std::vector<GpuInfo> GetSupportedCards();

    //IExternalPOW* create_opencl_solver(const std::vector<int32_t>& devices);
}