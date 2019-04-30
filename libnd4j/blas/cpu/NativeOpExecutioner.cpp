/*******************************************************************************
 * Copyright (c) 2015-2018 Skymind, Inc.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License, Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0.
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 ******************************************************************************/


#include <vector>
#include <pointercast.h>
#include "NativeOpExecutioner.h"
#include <types/types.h>

#include <pairwise_bool.h>
#include <broadcasting_bool.h>
#include <scalar_bool.h>

#include <loops/transform_float.h>
#include <loops/transform_bool.h>
#include <loops/transform_any.h>
#include <loops/transform_same.h>
#include <loops/transform_strict.h>

#include <loops/reduce_float.h>
#include <loops/reduce_same.h>
#include <loops/reduce_bool.h>
#include <loops/reduce_long.h>

#include <loops/broadcasting.h>
#include <loops/indexreduce.h>
#include <loops/pairwise_transform.h>
#include <loops/reduce_float.h>
#include <loops/reduce3.h>
#include <loops/summarystatsreduce.h>
#include <loops/transform_same.h>
#include <loops/scalar.h>
#include <loops/random.h>
#include <pointercast.h>
#include <exceptions/datatype_exception.h>




////////////////////////////////////////////////////////////////////////
/**
*
* @param opNum
* @param hX
* @param hXShapeInfo
* @param extraParams
* @param hZ
* @param hZShapeInfo
*/
void NativeOpExecutioner::execIndexReduceScalar(nd4j::graph::LaunchContext *lc, int opNum, 
                                    void *hX, Nd4jLong *hXShapeInfo,
                                    void *dX, Nd4jLong *dXShapeInfo,
                                    void *extraParams,
                                    void *hZ, Nd4jLong *hZShapeInfo,
                                    void *dZ, Nd4jLong *dZShapeInfo) {

    auto xType = nd4j::ArrayOptions::dataType(hXShapeInfo);
    auto hz = reinterpret_cast<Nd4jLong*>(hZ);

    BUILD_SINGLE_SELECTOR(xType, hz[0] = functions::indexreduce::IndexReduce, ::execScalar(opNum,hX,hXShapeInfo,extraParams), LIBND4J_TYPES);
}

////////////////////////////////////////////////////////////////////////
/**
 *
 * @param opNum
 * @param hX
 * @param hXShapeInfo
 * @param extraParams
 * @param hZ
 * @param hZShapeInfo
 * @param dimension
 * @param dimensionLength
 */

void NativeOpExecutioner::execIndexReduce(nd4j::graph::LaunchContext *lc,
                                int opNum,
                                void *hX, Nd4jLong *hXShapeInfo,
                                void *dX, Nd4jLong *dXShapeInfo,
                                void *extraParams,
                                void *hZ, Nd4jLong *hZShapeInfo,
                                void *dZ, Nd4jLong *dZShapeInfo,
                                int *dimension, int dimensionLength,
                                Nd4jLong *tadShapeInfo, Nd4jLong *tadOffsets) {

    auto xType = nd4j::ArrayOptions::dataType(hXShapeInfo);
    Nd4jLong* hz = reinterpret_cast<Nd4jLong*>(hZ);

    BUILD_SINGLE_SELECTOR(xType, functions::indexreduce::IndexReduce, ::exec(opNum, hX, hXShapeInfo, extraParams, hz, hZShapeInfo, dimension, dimensionLength, tadShapeInfo, tadOffsets), LIBND4J_TYPES);
//    BUILD_SINGLE_SELECTOR(xType, functions::indexreduce::IndexReduce, ::exec(opNum, hX, hXShapeInfo, dX, dXShapeInfo, extraParams, hZ, hZShapeInfo, dZ, dZShapeInfo, dimension, dimensionLength, tadShapeInfo, tadOffsets), LIBND4J_TYPES);
}

////////////////////////////////////////////////////////////////////////
/**
 *
 * @param opNum
 * @param hX
 * @param hXShapeInfo
 * @param hY
 * @param hYShapeInfo
 * @param hZ
 * @param hZShapeInfo
 * @param dimension
 * @param dimensionLength
 */

void NativeOpExecutioner::execBroadcast(nd4j::graph::LaunchContext *lc,
                            int opNum,
                            void *hX, Nd4jLong *hXShapeInfo,
                            void *dX, Nd4jLong *dXShapeInfo,
                            void *hY, Nd4jLong *hYShapeInfo,
                            void *dY, Nd4jLong *dYShapeInfo,
                            void *hZ, Nd4jLong *hZShapeInfo,
                            void *dZ, Nd4jLong *dZShapeInfo,
                            int *dimension, int dimensionLength,
                            Nd4jLong *tadOnlyShapeInfo, Nd4jLong *tadOffsets,
                            Nd4jLong *tadOnlyShapeInfoZ,Nd4jLong *tadOffsetsZ) {

    auto xType = nd4j::ArrayOptions::dataType(hXShapeInfo);
    auto yType = nd4j::ArrayOptions::dataType(hYShapeInfo);
    auto zType = nd4j::ArrayOptions::dataType(hZShapeInfo);

#ifdef __ND4J_EXPERIMENTAL__
    BUILD_PAIRWISE_SELECTOR(xType, yType, zType, functions::broadcast::Broadcast, ::exec(opNum, hX, hXShapeInfo, hY, hYShapeInfo, hZ, hZShapeInfo, dimension, dimensionLength, tadOnlyShapeInfo, tadOffsets, tadOnlyShapeInfoZ, tadOffsetsZ), LIBND4J_TYPES, LIBND4J_TYPES);
#else
    BUILD_SINGLE_SELECTOR_THRICE(xType, functions::broadcast::Broadcast, ::exec(opNum, hX, hXShapeInfo, hY, hYShapeInfo, hZ, hZShapeInfo, dimension, dimensionLength, tadOnlyShapeInfo, tadOffsets, tadOnlyShapeInfoZ, tadOffsetsZ), LIBND4J_TYPES);
#endif
}


////////////////////////////////////////////////////////////////////////
void NativeOpExecutioner::execBroadcastBool(nd4j::graph::LaunchContext *lc,
                            int opNum,
                            void *hX, Nd4jLong *hXShapeInfo,
                            void *dX, Nd4jLong *dXShapeInfo,
                            void *hY, Nd4jLong *hYShapeInfo,
                            void *dY, Nd4jLong *dYShapeInfo,
                            void *hZ, Nd4jLong *hZShapeInfo,
                            void *dZ, Nd4jLong *dZShapeInfo,
                            int *dimension, int dimensionLength,
                            Nd4jLong *tadOnlyShapeInfo, Nd4jLong *tadOffsets,
                            Nd4jLong *tadOnlyShapeInfoZ,Nd4jLong *tadOffsetsZ) {

    auto xType = nd4j::ArrayOptions::dataType(hXShapeInfo);
    auto yType = nd4j::ArrayOptions::dataType(hYShapeInfo);
    auto zType = nd4j::ArrayOptions::dataType(hZShapeInfo);

    BUILD_DOUBLE_SELECTOR(xType, zType, functions::broadcast::BroadcastBool, ::exec(opNum, hX, hXShapeInfo, hY, hYShapeInfo, hZ, hZShapeInfo, dimension, dimensionLength, tadOnlyShapeInfo, tadOffsets, tadOnlyShapeInfoZ, tadOffsetsZ), LIBND4J_TYPES, BOOL_TYPES);
}

////////////////////////////////////////////////////////////////////////
/**
*
* @param opNum
* @param hX
* @param xStride
* @param hY
* @param yStride
* @param hZ
* @param resultStride
* @param extraParams
* @param n
*/
void NativeOpExecutioner::execPairwiseTransform(nd4j::graph::LaunchContext *lc,
                                    int opNum,
                                    void *hX, Nd4jLong *hXShapeInfo,
                                    void *dX, Nd4jLong *dXShapeInfo,
                                    void *hY, Nd4jLong *hYShapeInfo,
                                    void *dY, Nd4jLong *dYShapeInfo,
                                    void *hZ, Nd4jLong *hZShapeInfo,
                                    void *dZ, Nd4jLong *dZShapeInfo,
                                    void *extraParams) {

    auto xType = nd4j::ArrayOptions::dataType(hXShapeInfo);
    auto yType = nd4j::ArrayOptions::dataType(hYShapeInfo);
    auto zType = nd4j::ArrayOptions::dataType(hZShapeInfo);

#ifdef __ND4J_EXPERIMENTAL__
    BUILD_PAIRWISE_SELECTOR(xType, yType, zType, functions::pairwise_transforms::PairWiseTransform, ::exec(opNum, hX, hXShapeInfo, hY, hYShapeInfo, hZ, hZShapeInfo, extraParams), LIBND4J_TYPES, LIBND4J_TYPES);
#else
    BUILD_SINGLE_SELECTOR_THRICE(xType, functions::pairwise_transforms::PairWiseTransform, ::exec(opNum, hX, hXShapeInfo, hY, hYShapeInfo, hZ, hZShapeInfo, extraParams), LIBND4J_TYPES);
#endif
}

////////////////////////////////////////////////////////////////////////
void NativeOpExecutioner::execPairwiseBoolTransform(nd4j::graph::LaunchContext *lc,
                                    int opNum,
                                    void *hX, Nd4jLong *hXShapeInfo,
                                    void *dX, Nd4jLong *dXShapeInfo,
                                    void *hY, Nd4jLong *hYShapeInfo,
                                    void *dY, Nd4jLong *dYShapeInfo,
                                    void *hZ, Nd4jLong *hZShapeInfo,
                                    void *dZ, Nd4jLong *dZShapeInfo,
                                    void *extraParams) {

    auto xType = nd4j::ArrayOptions::dataType(hXShapeInfo);
    auto yType = nd4j::ArrayOptions::dataType(hYShapeInfo);
    auto zType = nd4j::ArrayOptions::dataType(hZShapeInfo);

    BUILD_DOUBLE_SELECTOR(xType, zType, functions::pairwise_transforms::PairWiseBoolTransform, ::exec(opNum, hX, hXShapeInfo, hY, hYShapeInfo, hZ, hZShapeInfo, extraParams), LIBND4J_TYPES, BOOL_TYPES);
}

////////////////////////////////////////////////////////////////////////
/**
*
* @param opNum
* @param hX
* @param hXShapeInfo
* @param extraParams
* @param hZ
* @param hZShapeInfo
*/
void NativeOpExecutioner::execReduceFloat(nd4j::graph::LaunchContext *lc,
                            int opNum,
                            void *hX, Nd4jLong *hXShapeInfo,
                            void *dX, Nd4jLong *dXShapeInfo,
                            void *extraParams,
                            void *hZ, Nd4jLong *hZShapeInfo,
                            void *dZ, Nd4jLong *dZShapeInfo,
                            int *dimension, int dimensionLength,
                            Nd4jLong *tadShapeInfo, Nd4jLong *tadOffsets) {

    auto xType = nd4j::ArrayOptions::dataType(hXShapeInfo);
    auto zType = nd4j::ArrayOptions::dataType(hZShapeInfo);

    BUILD_DOUBLE_SELECTOR(xType, zType, functions::reduce::ReduceFloatFunction, ::exec(opNum, hX, hXShapeInfo, extraParams, hZ, hZShapeInfo, dimension, dimensionLength, tadShapeInfo, tadOffsets), LIBND4J_TYPES, FLOAT_TYPES);
}

////////////////////////////////////////////////////////////////////////
void NativeOpExecutioner::execReduceSame(nd4j::graph::LaunchContext *lc,
                                int opNum,
                                void *hX, Nd4jLong *hXShapeInfo,
                                void *dX, Nd4jLong *dXShapeInfo,
                                void *extraParams,
                                void *hZ, Nd4jLong *hZShapeInfo,
                                void *dZ, Nd4jLong *dZShapeInfo,
                                int *dimension, int dimensionLength,
                                Nd4jLong *tadShapeInfo, Nd4jLong *tadOffsets) {

    auto xType = nd4j::ArrayOptions::dataType(hXShapeInfo);
    auto zType = nd4j::ArrayOptions::dataType(hZShapeInfo);

    BUILD_SINGLE_SELECTOR(xType, functions::reduce::ReduceSameFunction, ::exec(opNum, hX, hXShapeInfo, extraParams, hZ, hZShapeInfo, dimension, dimensionLength, tadShapeInfo, tadOffsets), LIBND4J_TYPES);
}

////////////////////////////////////////////////////////////////////////
void NativeOpExecutioner::execReduceBool(nd4j::graph::LaunchContext *lc,
                                int opNum,
                                void *hX, Nd4jLong *hXShapeInfo,
                                void *dX, Nd4jLong *dXShapeInfo,
                                void *extraParams,
                                void *hZ, Nd4jLong *hZShapeInfo,
                                void *dZ, Nd4jLong *dZShapeInfo,
                                int *dimension, int dimensionLength,
                                Nd4jLong *tadShapeInfo, Nd4jLong *tadOffsets) {

    auto xType = nd4j::ArrayOptions::dataType(hXShapeInfo);
    auto zType = nd4j::ArrayOptions::dataType(hZShapeInfo);

    BUILD_DOUBLE_SELECTOR(xType, zType, functions::reduce::ReduceBoolFunction, ::exec(opNum, hX, hXShapeInfo, extraParams, hZ, hZShapeInfo, dimension, dimensionLength, tadShapeInfo, tadOffsets), LIBND4J_TYPES, BOOL_TYPES);
}

////////////////////////////////////////////////////////////////////////
void NativeOpExecutioner::execReduceLong(nd4j::graph::LaunchContext *lc,
                                int opNum,
                                void *hX, Nd4jLong *hXShapeInfo,
                                void *dX, Nd4jLong *dXShapeInfo,
                                void *extraParams,
                                void *hZ, Nd4jLong *hZShapeInfo,
                                void *dZ, Nd4jLong *dZShapeInfo,
                                int *dimension, int dimensionLength,
                                Nd4jLong *tadShapeInfo, Nd4jLong *tadOffsets) {

    auto xType = nd4j::ArrayOptions::dataType(hXShapeInfo);
    auto zType = nd4j::ArrayOptions::dataType(hZShapeInfo);

    BUILD_DOUBLE_SELECTOR(xType, zType, functions::reduce::ReduceLongFunction, ::exec(opNum, hX, hXShapeInfo, extraParams, hZ, hZShapeInfo, dimension, dimensionLength, tadShapeInfo, tadOffsets), LIBND4J_TYPES, LONG_TYPES);
}

////////////////////////////////////////////////////////////////////////
/**
 *
 * @param opNum
 * @param hX
 * @param hXShapeInfo
 * @param extraParams
 * @return
 */
void NativeOpExecutioner::execReduceFloatScalar(nd4j::graph::LaunchContext *lc,
                                    int opNum,
                                    void *hX, Nd4jLong *hXShapeInfo,
                                    void *dX, Nd4jLong *dXShapeInfo,
                                    void *extraParams,
                                    void *hZ, Nd4jLong *hZShapeInfo,
                                    void *dZ, Nd4jLong *dZShapeInfo) {

    auto xType = nd4j::ArrayOptions::dataType(hXShapeInfo);
    auto zType = nd4j::ArrayOptions::dataType(hZShapeInfo);

    BUILD_DOUBLE_SELECTOR(xType, zType, functions::reduce::ReduceFloatFunction, ::execScalar(opNum, hX, hXShapeInfo, extraParams, hZ, hZShapeInfo), LIBND4J_TYPES, FLOAT_TYPES);
}

////////////////////////////////////////////////////////////////////////
void NativeOpExecutioner::execReduceSameScalar(nd4j::graph::LaunchContext *lc,
                                        int opNum,
                                        void *hX, Nd4jLong *hXShapeInfo,
                                        void *dX, Nd4jLong *dXShapeInfo,
                                        void *extraParams,
                                        void *hZ, Nd4jLong *hZShapeInfo,
                                        void *dZ, Nd4jLong *dZShapeInfo) {

    auto xType = nd4j::ArrayOptions::dataType(hXShapeInfo);

    BUILD_SINGLE_SELECTOR(xType, functions::reduce::ReduceSameFunction, ::execScalar(opNum, hX, hXShapeInfo, extraParams, hZ, hZShapeInfo), LIBND4J_TYPES);
}

////////////////////////////////////////////////////////////////////////
void NativeOpExecutioner::execReduceBoolScalar(nd4j::graph::LaunchContext *lc,
                                        int opNum,
                                        void *hX, Nd4jLong *hXShapeInfo,
                                        void *dX, Nd4jLong *dXShapeInfo,
                                        void *extraParams,
                                        void *hZ, Nd4jLong *hZShapeInfo,
                                        void *dZ, Nd4jLong *dZShapeInfo) {

    auto xType = nd4j::ArrayOptions::dataType(hXShapeInfo);
    auto zType = nd4j::ArrayOptions::dataType(hZShapeInfo);

    BUILD_DOUBLE_SELECTOR(xType, zType, functions::reduce::ReduceBoolFunction, ::execScalar(opNum, hX, hXShapeInfo, extraParams, hZ, hZShapeInfo), LIBND4J_TYPES, BOOL_TYPES);
}

////////////////////////////////////////////////////////////////////////
void NativeOpExecutioner::execReduceLongScalar(nd4j::graph::LaunchContext *lc,
                                        int opNum,
                                        void *hX, Nd4jLong *hXShapeInfo,
                                        void *dX, Nd4jLong *dXShapeInfo,
                                        void *extraParams,
                                        void *hZ, Nd4jLong *hZShapeInfo,
                                        void *dZ, Nd4jLong *dZShapeInfo) {

    auto xType = nd4j::ArrayOptions::dataType(hXShapeInfo);
    auto zType = nd4j::ArrayOptions::dataType(hZShapeInfo);

    BUILD_DOUBLE_SELECTOR(xType, zType, functions::reduce::ReduceLongFunction, ::execScalar(opNum, hX, hXShapeInfo, extraParams, hZ, hZShapeInfo), LIBND4J_TYPES, LONG_TYPES);
}


////////////////////////////////////////////////////////////////////////
/**
 *
 * @param opNum
 * @param hX
 * @param hXShapeInfo
 * @param extraParamsVals
 * @param hY
 * @param hYShapeInfo
 * @param hZ
 * @param hZShapeInfo
 * @param dimension
 * @param dimensionLength
 */
void NativeOpExecutioner::execReduce3Scalar(nd4j::graph::LaunchContext *lc,
                            int opNum,
                            void *hX, Nd4jLong *hXShapeInfo,
                            void *dX, Nd4jLong *dXShapeInfo,
                            void *extraParamsVals,
                            void *hY, Nd4jLong *hYShapeInfo,
                            void *dY, Nd4jLong *dYShapeInfo,
                            void *hZ, Nd4jLong *hZShapeInfo,
                            void *dZ, Nd4jLong *dZShapeInfo) {

    auto xType = nd4j::ArrayOptions::dataType(hXShapeInfo);
    auto zType = nd4j::ArrayOptions::dataType(hZShapeInfo);


    BUILD_DOUBLE_SELECTOR(xType, zType, functions::reduce3::Reduce3, ::execScalar(opNum, hX, hXShapeInfo, extraParamsVals, hY, hYShapeInfo, hZ, hZShapeInfo), LIBND4J_TYPES, FLOAT_TYPES);
}

////////////////////////////////////////////////////////////////////////
/**
*
* @param opNum
* @param hX
* @param hXShapeInfo
* @param extraParamsVals
* @param hY
* @param hYShapeInfo
* @param hZ
* @param hZShapeInfo
*/
void NativeOpExecutioner::execReduce3(nd4j::graph::LaunchContext *lc,
                            int opNum,
                            void *hX, Nd4jLong *hXShapeInfo,
                            void *dX, Nd4jLong *dXShapeInfo,
                            void *extraParamsVals,
                            void *hY, Nd4jLong *hYShapeInfo,
                            void *dY, Nd4jLong *dYShapeInfo,
                            void *hZ, Nd4jLong *hZShapeInfo,
                            void *dZ, Nd4jLong *dZShapeInfo) {

    auto xType = nd4j::ArrayOptions::dataType(hXShapeInfo);
    auto zType = nd4j::ArrayOptions::dataType(hZShapeInfo);

    BUILD_DOUBLE_SELECTOR(xType, zType, functions::reduce3::Reduce3, ::exec(opNum, hX, hXShapeInfo, extraParamsVals, hY, hYShapeInfo, hZ, hZShapeInfo, nullptr, 1), LIBND4J_TYPES, FLOAT_TYPES);

}

////////////////////////////////////////////////////////////////////////
void NativeOpExecutioner::execReduce3(nd4j::graph::LaunchContext *lc,
                            int opNum,
                            void *hX, Nd4jLong *hXShapeInfo,
                            void *dX, Nd4jLong *dXShapeInfo,
                            void *extraParamsVals,
                            void *hY, Nd4jLong *hYShapeInfo,
                            void *dY, Nd4jLong *dYShapeInfo,
                            void *hZ, Nd4jLong *hZShapeInfo,
                            void *dZ, Nd4jLong *dZShapeInfo,
                            int *dimension, int dimensionLength,
                            Nd4jLong *xTadOnlyShapeInfo, Nd4jLong *xTadOffsets,
                            Nd4jLong *yTadOnlyShapeInfo, Nd4jLong *yTadOffsets) {

    auto xType = nd4j::ArrayOptions::dataType(hXShapeInfo);
    auto zType = nd4j::ArrayOptions::dataType(hZShapeInfo);

    BUILD_DOUBLE_SELECTOR(xType, zType, functions::reduce3::Reduce3, ::exec(opNum, hX, hXShapeInfo, extraParamsVals, hY, hYShapeInfo, hZ, hZShapeInfo, dimension, dimensionLength), LIBND4J_TYPES, FLOAT_TYPES);
}


////////////////////////////////////////////////////////////////////////
void NativeOpExecutioner::execReduce3All(nd4j::graph::LaunchContext *lc,
                            int opNum,
                            void *hX, Nd4jLong *hXShapeInfo,
                            void *dX, Nd4jLong *dXShapeInfo,
                            void *extraParamsVals,
                            void *hY, Nd4jLong *hYShapeInfo,
                            void *dY, Nd4jLong *dYShapeInfo,
                            void *hZ, Nd4jLong *hZShapeInfo,
                            void *dZ, Nd4jLong *dZShapeInfo,
                            int *dimension, int dimensionLength,
                            Nd4jLong *xTadShapeInfo, Nd4jLong *xOffsets,
                            Nd4jLong *yTadShapeInfo, Nd4jLong *yOffsets) {

    auto xType = nd4j::ArrayOptions::dataType(hXShapeInfo);
    auto zType = nd4j::ArrayOptions::dataType(hZShapeInfo);

    BUILD_DOUBLE_SELECTOR(xType, zType, functions::reduce3::Reduce3, ::execAll(opNum, hX, hXShapeInfo, extraParamsVals, hY, hYShapeInfo, hZ, hZShapeInfo, dimension, dimensionLength, xTadShapeInfo, xOffsets, yTadShapeInfo, yOffsets), LIBND4J_TYPES, FLOAT_TYPES);
//    BUILD_DOUBLE_SELECTOR(xType, zType, functions::reduce3::Reduce3, ::execAll(opNum, hX, hXShapeInfo, dX, dXShapeInfo, extraParamsVals, hY, hYShapeInfo, dY, dYShapeInfo, hZ, hZShapeInfo, dZ, dZShapeInfo, dimension, dimensionLength, xTadShapeInfo, xOffsets, yTadShapeInfo, yOffsets), LIBND4J_TYPES, FLOAT_TYPES);
}

////////////////////////////////////////////////////////////////////////
void NativeOpExecutioner::execReduce3TAD(nd4j::graph::LaunchContext *lc,
                            int opNum,
                            void *hX, Nd4jLong *hXShapeInfo,
                            void *dX, Nd4jLong *dXShapeInfo,
                            void *extraParamsVals,
                            void *hY, Nd4jLong *hYShapeInfo,
                            void *dY, Nd4jLong *dYShapeInfo,
                            void *hZ, Nd4jLong *hZShapeInfo,
                            void *dZ, Nd4jLong *dZShapeInfo,
                            int *dimension, int dimensionLength, 
                            Nd4jLong *tadShapeInfo, Nd4jLong *tadOffsets) {

    auto xType = nd4j::ArrayOptions::dataType(hXShapeInfo);
    auto zType = nd4j::ArrayOptions::dataType(hZShapeInfo);

    BUILD_DOUBLE_SELECTOR(xType, zType, functions::reduce3::Reduce3, ::exec(opNum, hX, hXShapeInfo, extraParamsVals, hY, hYShapeInfo, hZ, hZShapeInfo, dimension, dimensionLength, tadShapeInfo, tadOffsets), LIBND4J_TYPES, FLOAT_TYPES);
//    BUILD_DOUBLE_SELECTOR(xType, zType, functions::reduce3::Reduce3, ::exec(opNum, hX, hXShapeInfo, dX, dXShapeInfo, extraParamsVals, hY, hYShapeInfo, dY, dYShapeInfo, hZ, hZShapeInfo, dZ, dZShapeInfo, dimension, dimensionLength, tadShapeInfo, tadOffsets), LIBND4J_TYPES, FLOAT_TYPES);
}


////////////////////////////////////////////////////////////////////////
/**
*
* @param opNum
* @param hX
* @param xStride
* @param hZ
* @param resultStride
* @param scalar
* @param extraParams
* @param n
*/
void NativeOpExecutioner::execScalar(nd4j::graph::LaunchContext *lc,
                            int opNum,
                            void *hX, Nd4jLong *hXShapeInfo,
                            void *dX, Nd4jLong *dXShapeInfo,
                            void *hZ, Nd4jLong *hZShapeInfo,
                            void *dZ, Nd4jLong *dZShapeInfo,
                            void *hScalar, Nd4jLong *hScalarShapeInfo,
                            void *dScalar, Nd4jLong *dScalarShapeInfo,
                            void *extraParams) {

    auto xType = nd4j::ArrayOptions::dataType(hXShapeInfo);
    auto yType = nd4j::ArrayOptions::dataType(hScalarShapeInfo);
    auto zType = nd4j::ArrayOptions::dataType(hZShapeInfo);

#ifdef __ND4J_EXPERIMENTAL__
    BUILD_PAIRWISE_SELECTOR(xType, yType, zType, functions::scalar::ScalarTransform, ::transform(opNum, hX, hXShapeInfo, hZ, hZShapeInfo, hScalar, extraParams), LIBND4J_TYPES, LIBND4J_TYPES);
#else
    if (xType != yType || xType != zType)
        throw nd4j::datatype_exception::build("NativeOpExecutioner::execScalar", zType, xType, yType);

    BUILD_SINGLE_SELECTOR_THRICE(xType, functions::scalar::ScalarTransform, ::transform(opNum, hX, hXShapeInfo, hZ, hZShapeInfo, hScalar, extraParams), LIBND4J_TYPES);
#endif
}

////////////////////////////////////////////////////////////////////////
void NativeOpExecutioner::execScalar(nd4j::graph::LaunchContext *lc,
                            int opNum,
                            void *hX, Nd4jLong *hXShapeInfo,
                            void *dX, Nd4jLong *dXShapeInfo,
                            void *extraParams,
                            void *hZ, Nd4jLong *hZShapeInfo,
                            void *dZ, Nd4jLong *dZShapeInfo,
                            void *hScalars, Nd4jLong *hScalarShapeInfo,
                            void *dScalars, Nd4jLong *dScalarShapeInfo,
                            int *dimension, int dimensionLength,
                            Nd4jLong *tadShapeInfo, Nd4jLong *tadOffsets,
                            Nd4jLong *tadShapeInfoZ, Nd4jLong *tadOffsetsZ) {

    auto xType = nd4j::ArrayOptions::dataType(hXShapeInfo);
    auto yType = nd4j::ArrayOptions::dataType(hScalarShapeInfo);
    auto zType = nd4j::ArrayOptions::dataType(hZShapeInfo);

#ifdef __ND4J_EXPERIMENTAL__
    BUILD_PAIRWISE_SELECTOR(xType, yType, zType, functions::scalar::ScalarTransform, ::transform(opNum, hX, hXShapeInfo, extraParams, hZ, hZShapeInfo, hScalars, dimension, dimensionLength, tadShapeInfo, tadOffsets, tadShapeInfoZ, tadOffsetsZ), LIBND4J_TYPES, LIBND4J_TYPES);
#else
    if (xType != yType || xType != zType)
        throw nd4j::datatype_exception::build("NativeOpExecutioner::execScalar", zType, xType, yType);

    BUILD_SINGLE_SELECTOR_THRICE(xType, functions::scalar::ScalarTransform, ::transform(opNum, hX, hXShapeInfo, extraParams, hZ, hZShapeInfo, hScalars, dimension, dimensionLength, tadShapeInfo, tadOffsets, tadShapeInfoZ, tadOffsetsZ), LIBND4J_TYPES);
#endif
}

////////////////////////////////////////////////////////////////////////
void NativeOpExecutioner::execScalarBool(nd4j::graph::LaunchContext *lc,
                            int opNum,
                            void *hX, Nd4jLong *hXShapeInfo,
                            void *dX, Nd4jLong *dXShapeInfo,
                            void *hZ, Nd4jLong *hZShapeInfo,
                            void *dZ, Nd4jLong *dZShapeInfo,
                            void *hScalar, Nd4jLong *hSscalarShapeInfo,
                            void *dScalar, Nd4jLong *dSscalarShapeInfo,
                            void *extraParams) {

    auto xType = nd4j::ArrayOptions::dataType(hXShapeInfo);
    auto yType = nd4j::ArrayOptions::dataType(hSscalarShapeInfo);
    auto zType = nd4j::ArrayOptions::dataType(hZShapeInfo);

    if (xType != yType)
        throw nd4j::datatype_exception::build("NativeOpExecutioner::execScalarBool", xType, yType);

    if (zType != nd4j::DataType::BOOL)
        throw nd4j::datatype_exception::build("NativeOpExecutioner::execScalarBool", nd4j::DataType::BOOL, zType);

    BUILD_DOUBLE_SELECTOR(xType, zType, functions::scalar::ScalarBoolTransform, ::transform(opNum, hX, hXShapeInfo, hZ, hZShapeInfo, hScalar, extraParams), LIBND4J_TYPES, BOOL_TYPES);
}

////////////////////////////////////////////////////////////////////////
void NativeOpExecutioner::execScalarBool(nd4j::graph::LaunchContext *lc,
                            int opNum,
                            void *hX, Nd4jLong *hXShapeInfo,
                            void *dX, Nd4jLong *dXShapeInfo,
                            void *extraParams,
                            void *hZ, Nd4jLong *hZShapeInfo,
                            void *dZ, Nd4jLong *dZShapeInfo,
                            void *hScalars, Nd4jLong *hScalarShapeInfo,
                            void *dScalars, Nd4jLong *dScalarShapeInfo,
                            int *dimension, int dimensionLength,
                            Nd4jLong *tadShapeInfo, Nd4jLong *tadOffsets,
                            Nd4jLong *tadShapeInfoZ, Nd4jLong *tadOffsetsZ) {

    auto xType = nd4j::ArrayOptions::dataType(hXShapeInfo);
    auto yType = nd4j::ArrayOptions::dataType(hScalarShapeInfo);
    auto zType = nd4j::ArrayOptions::dataType(hZShapeInfo);

    if (xType != yType)
        throw nd4j::datatype_exception::build("NativeOpExecutioner::execScalarBool", xType, yType);

    if (zType != nd4j::DataType::BOOL)
        throw nd4j::datatype_exception::build("NativeOpExecutioner::execScalarBool", nd4j::DataType::BOOL, zType);

    BUILD_DOUBLE_SELECTOR(xType, zType, functions::scalar::ScalarBoolTransform, ::transform(opNum, hX, hXShapeInfo, extraParams, hZ, hZShapeInfo, hScalars, dimension, dimensionLength, tadShapeInfo, tadOffsets, tadShapeInfoZ, tadOffsetsZ), LIBND4J_TYPES, BOOL_TYPES);
}

////////////////////////////////////////////////////////////////////////
/**
*
* @param opNum
* @param hX
* @param hXShapeInfo
* @param extraParams
* @param hZ
* @param hZShapeInfo
*/
void NativeOpExecutioner::execSummaryStats(nd4j::graph::LaunchContext *lc,
                                int opNum,
                                void *hX, Nd4jLong *hXShapeInfo,
                                void *dX, Nd4jLong *dXShapeInfo,
                                void *extraParams,
                                void *hZ, Nd4jLong *hZShapeInfo,
                                void *dZ, Nd4jLong *dZShapeInfo,
                                bool biasCorrected) {

    auto xType = nd4j::ArrayOptions::dataType(hXShapeInfo);
    auto zType = nd4j::ArrayOptions::dataType(hZShapeInfo);

    BUILD_DOUBLE_SELECTOR(xType, zType, functions::summarystats::SummaryStatsReduce, ::exec(opNum, biasCorrected, hX, hXShapeInfo, extraParams, hZ, hZShapeInfo, nullptr, 1), LIBND4J_TYPES, FLOAT_TYPES);
}

////////////////////////////////////////////////////////////////////////
/**
*
* @param opNum
* @param hX
* @param hXShapeInfo
* @param extraParams
* @param hZ
* @param hZShapeInfo
*/
void NativeOpExecutioner::execSummaryStatsScalar(nd4j::graph::LaunchContext *lc,
                                    int opNum,
                                    void *hX, Nd4jLong *hXShapeInfo,
                                    void *dX, Nd4jLong *dXShapeInfo,
                                    void *extraParams,
                                    void *hZ, Nd4jLong *hZShapeInfo,
                                    void *dZ, Nd4jLong *dZShapeInfo,
                                    bool biasCorrected) {

    auto xType = nd4j::ArrayOptions::dataType(hXShapeInfo);
    auto zType = nd4j::ArrayOptions::dataType(hZShapeInfo);

    BUILD_DOUBLE_SELECTOR(xType, zType, functions::summarystats::SummaryStatsReduce, ::execScalar(opNum, biasCorrected, hX, hXShapeInfo, extraParams, hZ, hZShapeInfo), LIBND4J_TYPES, FLOAT_TYPES);
}

////////////////////////////////////////////////////////////////////////
/**
*
* @param opNum
* @param hX
* @param hXShapeInfo
* @param extraParams
* @param hZ
* @param hZShapeInfo
* @param dimension
* @param dimensionLength
*/
void NativeOpExecutioner::execSummaryStats(nd4j::graph::LaunchContext *lc,
                                int opNum,
                                void *hX, Nd4jLong *hXShapeInfo,
                                void *dX, Nd4jLong *dXShapeInfo,
                                void *extraParams,
                                void *hZ, Nd4jLong *hZShapeInfo,
                                void *dZ, Nd4jLong *dZShapeInfo,
                                int *dimension, int dimensionLength,
                                Nd4jLong *tadShapeInfo, Nd4jLong *tadOffsets,
                                bool biasCorrected) {

    auto xType = nd4j::ArrayOptions::dataType(hXShapeInfo);
    auto zType = nd4j::ArrayOptions::dataType(hZShapeInfo);

    BUILD_DOUBLE_SELECTOR(xType, zType, functions::summarystats::SummaryStatsReduce, ::exec(opNum, biasCorrected, hX, hXShapeInfo, extraParams, hZ, hZShapeInfo, dimension, dimensionLength), LIBND4J_TYPES, FLOAT_TYPES);
}


////////////////////////////////////////////////////////////////////////
/**
*
* @param opNum
* @param hX
* @param xStride
* @param hZ
* @param resultStride
* @param extraParams
* @param n
*/
void NativeOpExecutioner::execTransformFloat(nd4j::graph::LaunchContext *lc,
                                int opNum,
                                void *hX, Nd4jLong *hXShapeInfo,
                                void *dX, Nd4jLong *dXShapeInfo,
                                void *hZ, Nd4jLong *hZShapeInfo,
                                void *dZ, Nd4jLong *dZShapeInfo,
                                void *extraParams,
                                Nd4jLong *tadShapeInfo, Nd4jLong *tadOffsets) {

    auto xType = nd4j::ArrayOptions::dataType(hXShapeInfo);
    auto zType = nd4j::ArrayOptions::dataType(hZShapeInfo);

    BUILD_DOUBLE_SELECTOR(xType, zType, functions::transform::TransformFloat, ::exec(opNum, hX, hXShapeInfo, hZ, hZShapeInfo, extraParams, tadShapeInfo, tadOffsets), LIBND4J_TYPES, FLOAT_TYPES);
}

////////////////////////////////////////////////////////////////////////
void NativeOpExecutioner::execTransformBool(nd4j::graph::LaunchContext *lc,
                                int opNum,
                                void *hX, Nd4jLong *hXShapeInfo,
                                void *dX, Nd4jLong *dXShapeInfo,
                                void *hZ, Nd4jLong *hZShapeInfo,
                                void *dZ, Nd4jLong *dZShapeInfo,
                                void *extraParams,
                                Nd4jLong *tadShapeInfo, Nd4jLong *tadOffsets) {

    auto xType = nd4j::ArrayOptions::dataType(hXShapeInfo);
    auto zType = nd4j::ArrayOptions::dataType(hZShapeInfo);

    BUILD_DOUBLE_SELECTOR(xType, zType, functions::transform::TransformBool, ::exec(opNum, hX, hXShapeInfo, hZ, hZShapeInfo, extraParams, tadShapeInfo, tadOffsets), LIBND4J_TYPES, BOOL_TYPES);
}

////////////////////////////////////////////////////////////////////////
void NativeOpExecutioner::execTransformAny(nd4j::graph::LaunchContext *lc,
                                int opNum,
                                void *hX, Nd4jLong *hXShapeInfo,
                                void *dX, Nd4jLong *dXShapeInfo,
                                void *hZ, Nd4jLong *hZShapeInfo,
                                void *dZ, Nd4jLong *dZShapeInfo,
                                void *extraParams,
                                Nd4jLong *tadShapeInfo, Nd4jLong *tadOffsets) {

    auto xType = nd4j::ArrayOptions::dataType(hXShapeInfo);
    auto zType = nd4j::ArrayOptions::dataType(hZShapeInfo);

    BUILD_DOUBLE_SELECTOR(xType, zType, functions::transform::TransformAny, ::exec(opNum, hX, hXShapeInfo, hZ, hZShapeInfo, extraParams, tadShapeInfo, tadOffsets), LIBND4J_TYPES, LIBND4J_TYPES);
}

////////////////////////////////////////////////////////////////////////
void NativeOpExecutioner::execTransformSame(nd4j::graph::LaunchContext *lc,
                                int opNum,
                                void *hX, Nd4jLong *hXShapeInfo,
                                void *dX, Nd4jLong *dXShapeInfo,
                                void *hZ, Nd4jLong *hZShapeInfo,
                                void *dZ, Nd4jLong *dZShapeInfo,
                                void *extraParams,
                                Nd4jLong *tadShapeInfo, Nd4jLong *tadOffsets) {

    auto xType = nd4j::ArrayOptions::dataType(hXShapeInfo);
    auto zType = nd4j::ArrayOptions::dataType(hZShapeInfo);

    BUILD_SINGLE_SELECTOR(xType, functions::transform::TransformSame, ::exec(opNum, hX, hXShapeInfo, hZ, hZShapeInfo, extraParams, tadShapeInfo, tadOffsets), LIBND4J_TYPES);
}

////////////////////////////////////////////////////////////////////////
void NativeOpExecutioner::execTransformStrict(nd4j::graph::LaunchContext *lc,
                                int opNum,
                                void *hX, Nd4jLong *hXShapeInfo,
                                void *dX, Nd4jLong *dXShapeInfo,
                                void *hZ, Nd4jLong *hZShapeInfo,
                                void *dZ, Nd4jLong *dZShapeInfo,
                                void *extraParams,
                                Nd4jLong *tadShapeInfo, Nd4jLong *tadOffsets) {

    auto xType = nd4j::ArrayOptions::dataType(hXShapeInfo);
    auto zType = nd4j::ArrayOptions::dataType(hZShapeInfo);

    BUILD_SINGLE_SELECTOR(xType, functions::transform::TransformStrict, ::exec(opNum, hX, hXShapeInfo, hZ, hZShapeInfo, extraParams, tadShapeInfo, tadOffsets), FLOAT_TYPES);
}

////////////////////////////////////////////////////////////////////////
void NativeOpExecutioner::execRandom(nd4j::graph::LaunchContext *lc,
                            int opNum,
                            Nd4jPointer state,
                            void *hZ, Nd4jLong *hZShapeInfo,
                            void *dZ, Nd4jLong *dZShapeInfo,
                            void *extraArguments) {

    auto zType = nd4j::ArrayOptions::dataType(hZShapeInfo);

    BUILD_SINGLE_SELECTOR(zType, functions::random::RandomFunction, ::execTransform(opNum, state, hZ, hZShapeInfo, extraArguments), FLOAT_TYPES);
}

////////////////////////////////////////////////////////////////////////
void NativeOpExecutioner::execRandom(nd4j::graph::LaunchContext *lc,
                            int opNum,
                            Nd4jPointer state,
                            void *hX, Nd4jLong *hXShapeInfo,
                            void *dX, Nd4jLong *dXShapeInfo,
                            void *hZ, Nd4jLong *hZShapeInfo,
                            void *dZ, Nd4jLong *dZShapeInfo,
                            void *extraArguments) {

    auto zType = nd4j::ArrayOptions::dataType(hZShapeInfo);

    BUILD_SINGLE_SELECTOR(zType, functions::random::RandomFunction, ::execTransform(opNum, state, hX, hXShapeInfo, hZ, hZShapeInfo, extraArguments), FLOAT_TYPES);
}

////////////////////////////////////////////////////////////////////////
void NativeOpExecutioner::execRandom(nd4j::graph::LaunchContext *lc,
                          int opNum,
                          Nd4jPointer state,
                          void *hX, Nd4jLong *hXShapeInfo,
                          void *dX, Nd4jLong *dXShapeInfo,
                          void *hY, Nd4jLong *hYShapeInfo,
                          void *dY, Nd4jLong *dYShapeInfo,
                          void *hZ, Nd4jLong *hZShapeInfo,
                          void *dZ, Nd4jLong *dZShapeInfo,
                          void *extraArguments) {

    auto xType = nd4j::ArrayOptions::dataType(hZShapeInfo);

    BUILD_SINGLE_SELECTOR(xType, functions::random::RandomFunction, ::execTransform(opNum, state, hX, hXShapeInfo, hY, hYShapeInfo, hZ, hZShapeInfo, extraArguments), FLOAT_TYPES);
}







