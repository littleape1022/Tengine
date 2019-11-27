/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * License); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * AS IS BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*
 * Copyright (c) 2019, Open AI Lab
 * Author: zpluo@openailab.com
 */
#ifndef __SPLIT_PARAM_HPP__
#define __SPLIT_PARAM_HPP__

#include "parameter.hpp"

namespace TEngine {

struct SplitParam : public NamedParam
{
    int axis;
    int split_dim;
    bool is_caffe;
    int squeeze_axis;
    std::vector<int> split_sizes_;

    DECLARE_PARSER_STRUCTURE(SplitParam)
    {
        DECLARE_PARSER_ENTRY(axis);
        DECLARE_PARSER_ENTRY(split_dim);
        DECLARE_PARSER_ENTRY(is_caffe);
        DECLARE_PARSER_ENTRY(squeeze_axis);
        DECLARE_PARSER_ENTRY(split_sizes_);
    }
};

}    // namespace TEngine

#endif