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

#ifndef NDARRAY_CPP
#define NDARRAY_CPP

#include "../NDArray.h"
#include "../NDArrayFactory.h"
#include "NativeOpExecutioner.h"
#include <BroadcastPairwiseConverter.h>
#include <memory/Workspace.h>
#include <memory/MemoryRegistrator.h>
#include <ops.h>
#include <ops/gemm.h>
#include <pointercast.h>
#include <stdexcept>
#include <memory>
#include <helpers/logger.h>
#include <loops/pairwise_transform.h>
#include <loops/transform_same.h>
#include <loops/random.h>
#include <loops/broadcasting.h>
#include <indexing/NDIndex.h>
#include <indexing/IndicesList.h>
#include <helpers/ShapeUtils.h>
#include <sstream>
#include <helpers/ArrayUtils.h>
#include <MmulHelper.h>
#include <helpers/threshold.h>
#include <exceptions/datatype_exception.h>
#include <exceptions/allocation_exception.h>
#include <helpers/ConstantTadHelper.h>

#include <NDArray.hpp>


namespace nd4j {


////////////////////////////////////////////////////////////////////////
template<typename T>
void NDArray::setValueInDiagMatrix(const T& value, const int diag, const char direction) {
    if (isS())
        throw std::runtime_error("NDArray::setValueInDiagMatrix: you can't use this method on String array!");
    if(rankOf() != 2)
       throw std::string("NDArray::setValueInDiagMatrix method: array must have rank = 2, but got " + toStringValue(rankOf()) + " instead !");

    const auto rows = sizeAt(0);
    const auto cols = sizeAt(1);

    switch(direction) {

        case 'u':                           // fill upper triangular block
#pragma omp parallel for if(rows > Environment::getInstance()->elementwiseThreshold()) schedule(guided) collapse (2)
            for(Nd4jLong i = 0; i < rows; ++i)
                for(Nd4jLong j = 0; j < cols; ++j)
                    if (i + diag <= j)
                        p<T>(i, j, value);
            break;

        case 'l':                           // fill lower triangular block
#pragma omp parallel for if(rows > Environment::getInstance()->elementwiseThreshold()) schedule(guided) collapse (2)
            for(Nd4jLong i = 0; i < rows; ++i)
                for(Nd4jLong j = 0; j < cols; ++j)
                    if (i + diag >= j)
                        p<T>(i, j, value);
            break;
        default:
            throw std::string("NDArray::setValueInDiagMatrix method: wrong value of direction argument, expected is 'u' or 'l', but got " + std::string(1,direction) + " instead !");
    }
}
template void NDArray::setValueInDiagMatrix(const double& value, const int diag, const char direction);
template void NDArray::setValueInDiagMatrix(const float& value, const int diag, const char direction);
template void NDArray::setValueInDiagMatrix(const float16& value, const int diag, const char direction);
template void NDArray::setValueInDiagMatrix(const bfloat16& value, const int diag, const char direction);
template void NDArray::setValueInDiagMatrix(const Nd4jLong& value, const int diag, const char direction);
template void NDArray::setValueInDiagMatrix(const int& value, const int diag, const char direction);
template void NDArray::setValueInDiagMatrix(const int16_t& value, const int diag, const char direction);
template void NDArray::setValueInDiagMatrix(const uint8_t& value, const int diag, const char direction);
template void NDArray::setValueInDiagMatrix(const int8_t& value, const int diag, const char direction);
template void NDArray::setValueInDiagMatrix(const bool& value, const int diag, const char direction);

////////////////////////////////////////////////////////////////////////
void NDArray::setIdentity() {
    if (isS())
        throw std::runtime_error("NDArray::setIdentity: you can't use this method on String array!");

    this->assign(0.);

    int  rank    = rankOf();
    auto shape   = shapeOf();
    auto strides = stridesOf();
    int  minDim  = MAX_INT;
    Nd4jLong indices[MAX_RANK];
    for(int j = 0; j < rank; ++j)
        indices[j] = 1;

    Nd4jLong offset = shape::getOffset(0, shape, strides, indices, rank);

    for(int i = 0; i < rank; ++i)
        if(minDim > shape[i])
            minDim = shape[i];

    float v = 1.0f;
#pragma omp parallel for if(minDim > Environment::getInstance()->elementwiseThreshold()) schedule(guided)
    for(int i = 0; i < minDim; ++i)
        templatedSet<float>(buffer(), i*offset, this->dataType(), &v);
}

////////////////////////////////////////////////////////////////////////
template <typename T>
static void templatedSwap(void *xBuffer, void *yBuffer, Nd4jLong length) {
    auto x = reinterpret_cast<T *>(xBuffer);
    auto y = reinterpret_cast<T *>(yBuffer);

#pragma omp parallel for simd schedule(static)
    for (int i = 0; i < length; ++i) {
        auto temp = x[i];
        x[i] = y[i];
        y[i] = temp;
    }
}
BUILD_SINGLE_TEMPLATE(template void templatedSwap, (void *xBuffer, void *yBuffer, Nd4jLong length), LIBND4J_TYPES);

////////////////////////////////////////////////////////////////////////
void NDArray::swapUnsafe(NDArray& other) {
    auto xType = this->dataType();

    if (xType != other.dataType())
        throw std::runtime_error("NDArray::swapUnsage method: both arrays must have the same data type");

    if(buffer() == nullptr || other.buffer() == nullptr)
        throw std::runtime_error("NDArray::swapUnsafe method: input array should not be empty!");

    if(lengthOf() != other.lengthOf())
        throw std::runtime_error("NDArray::swapUnsafe method: input arrays should have the same length!");

    BUILD_SINGLE_SELECTOR(xType, templatedSwap, (buffer(), other.buffer(), this->lengthOf()), LIBND4J_TYPES);
}

////////////////////////////////////////////////////////////////////////
void NDArray::synchronize() const {
    // no-op
}
void NDArray::prepareSpecialUse(const std::initializer_list<const NDArray*>& writeList, const std::initializer_list<const NDArray*>& readList, bool synchronizeWritables) {
    // no-op
}
void NDArray::registerSpecialUse(const std::initializer_list<const NDArray*>& writeList, const std::initializer_list<const NDArray*>& readList) {
    // no-op
}
void NDArray::preparePrimaryUse(const std::initializer_list<const NDArray*>& writeList, const std::initializer_list<const NDArray*>& readList, bool synchronizeWritables) {
    // no-op
}
void NDArray::registerPrimaryUse(const std::initializer_list<const NDArray*>& writeList, const std::initializer_list<const NDArray*>& readList) {
    // no-op
}
//////////////////////////////////////////////////////////////////////////
void NDArray::syncShape() const {
    // no-op
}

























    ////////////////////////////////////////////////////////////////////////
    // This method returns new copy of this NDArray, optionally in different order
    NDArray* NDArray::dup(const char newOrder) {
        if (isEmpty())
            return NDArrayFactory::empty_(this->dataType(), this->_context);

        char order = newOrder == 'a' ? ordering() : newOrder;

        // for now string arrays require special treatment
        if (this->dataType() == DataType::UTF8) {
            std::vector<std::string> strings(_length);
            for (int e = 0; e < _length; e++)
                strings[e] = this->e<std::string>(e);

            auto result = NDArrayFactory::string_(order, this->getShapeAsVector(), strings, _context);
            return result;
        } else {
            auto outShapeInfo = ConstantShapeHelper::getInstance()->createShapeInfo(_dataType, order, getShapeAsVector());
            void *outBuffer = nullptr;
            ALLOCATE(outBuffer, _context->getWorkspace(), _length * sizeOfT(), int8_t);

            auto result = new NDArray(outBuffer, outShapeInfo, _context, true);
            result->assign(this);

            return result;
        }
    }

////////////////////////////////////////////////////////////////////////
    // This method returns true if two arrays are equal, with custom or default Eps value of 1e-5, false otherwise
    bool NDArray::equalsTo(const NDArray *other, double eps) const {

        if (this->dataType() != other->dataType() || lengthOf() != other->lengthOf())
            return false;

        // we need to be able to compare [1, len] to [len]
        if ((rankOf() == 1 && other->rankOf() == 2) || (rankOf() == 2 && other->rankOf() == 1)) {
            // FIXME: do something here?
        }
        else if (!shape::equalsSoft(_shapeInfo, other->_shapeInfo))
            return false;

        NDArray tmp(nd4j::DataType::FLOAT32, _context); // scalar = 0

        ExtraArguments extras({eps});
        NativeOpExecutioner::execReduce3Scalar(_context, reduce3::EqualsWithEps, _buffer, _shapeInfo, _bufferD, _shapeInfoD, extras.argumentsAsT(DataType::FLOAT32), other->_buffer, other->_shapeInfo, other->_bufferD, other->_shapeInfoD, tmp.buffer(), tmp.shapeInfo(), tmp._bufferD, tmp._shapeInfoD);

        if (tmp.e<int>(0) > 0)
            return false;

        return true;
    }

//////////////////////////////////////////////////////////////////////////
    template <>
    utf8string NDArray::e(const Nd4jLong i) const {
        if (i >= _length)
            throw std::invalid_argument("NDArray::e(i): input index is out of array length !");

        if (!isS())
            throw std::runtime_error("This method is available for String arrays only");

        auto rp = getOffset(i);
        return *(reinterpret_cast<utf8string**>(_buffer)[rp]);
    }

    template <>
    std::string NDArray::e(const Nd4jLong i) const {
        if (!isS())
            throw std::runtime_error("Can't get std::string out of non-string array");

        // getting "virtual" offset. it's not real though,since it doesn't take lengths into account
        auto offset = getOffset(i);
        auto offsets = reinterpret_cast<Nd4jLong *>(_buffer);
        auto offsetsLength = ShapeUtils::stringBufferHeaderRequirements(this->lengthOf());
        auto start = offsets[offset];
        auto end = offsets[offset + 1];
        auto data = _buffer + offsetsLength + start;

        std::string r(reinterpret_cast<const char *>(data), (end - start));
        return r;
    }

    template <typename T>
    T NDArray::e(const Nd4jLong i) const {

        if (i >= _length)
            throw std::invalid_argument("NDArray::e(i): input index is out of array length !");

        auto rp = getOffset(i);

        BUILD_SINGLE_PARTIAL_SELECTOR(this->dataType(), return templatedGet<, T>(this->_buffer, rp), LIBND4J_TYPES);
//        return static_cast<T>(119);
    }
    BUILD_SINGLE_UNCHAINED_TEMPLATE(template , NDArray::e(const Nd4jLong) const, LIBND4J_TYPES);

////////////////////////////////////////////////////////////////////////
#ifndef __JAVACPP_HACK__

    template<typename T>
    void NDArray::applyTriplewiseLambda(NDArray* second, NDArray *third, const std::function<T(T, T, T)>& func, NDArray* target) {
        if (target == nullptr)
            target = this;

        if (second == nullptr) {
            nd4j_printf("applyTriplewiseLambda requires three operands to be valid NDArrays, but Second is NULL\n","");
            throw std::runtime_error("second is null");
        }

        if (third == nullptr) {
            nd4j_printf("applyTriplewiseLambda requires three operands to be valid NDArrays, but Third is NULL\n","");
            throw std::runtime_error("third is null");
        }
        if(_dataType != DataTypeUtils::fromT<T>())
            throw std::runtime_error("NDArray::applyTriplewiseLambda<T> method: wrong template parameter T, its type should be the same as type of this array!");
        if(_dataType != second->_dataType || _dataType != third->_dataType || _dataType != target->_dataType)
            throw std::runtime_error("NDArray::applyTriplewiseLambda<T> method: bother four arrays (this, second, third, target) should have the same type !");

        if (this->lengthOf() != second->lengthOf() || this->lengthOf() != third->lengthOf() || !this->isSameShape(second) || !this->isSameShape(third)) {
            nd4j_printf("applyPairwiseLambda requires both operands to have the same shape\n","");
            throw std::runtime_error("Shapes mismach");
        }

        auto f = this->bufferAsT<T>();
        auto s = second->bufferAsT<T>();
        auto t = third->bufferAsT<T>();
        auto z = target->bufferAsT<T>();

        if (this->ordering() == second->ordering() && this->ordering() == third->ordering()  && this->ordering() == target->ordering() && (this->ews() == 1 && target->ews() == 1) && this->ews() == second->ews() && this->ews() == third->ews()) {

            PRAGMA_OMP_PARALLEL_FOR_SIMD
            for (Nd4jLong e = 0; e < _length; e++)
                z[e] = func(f[e], s[e], t[e]);
        } else {
            if (f == z) {

                PRAGMA_OMP_PARALLEL_FOR_SIMD
                for (int e = 0; e < _length; e++) {

                    auto tOffset = this->getOffset(e);
                    auto uOffset = second->getOffset(e);
                    auto vOffset = third->getOffset(e);

                    f[tOffset] = func(f[tOffset], s[uOffset], t[vOffset]);
                }
            } else {

                PRAGMA_OMP_PARALLEL_FOR_SIMD
                for (int e = 0; e < _length; e++) {

                    auto tOffset = this->getOffset(e);
                    auto uOffset = second->getOffset(e);
                    auto vOffset = third->getOffset(e);
                    auto zOffset = target->getOffset(e);

                    z[zOffset] = func(f[tOffset], s[uOffset], t[vOffset]);
                }
            }
        }
    }
    template void NDArray::applyTriplewiseLambda(NDArray* second, NDArray *third, const std::function<double (double, double, double)>& func, NDArray* target);
    template void NDArray::applyTriplewiseLambda(NDArray* second, NDArray *third, const std::function<float (float, float, float)>& func, NDArray* target);
    template void NDArray::applyTriplewiseLambda(NDArray* second, NDArray *third, const std::function<float16 (float16, float16, float16)>& func, NDArray* target);
    template void NDArray::applyTriplewiseLambda(NDArray* second, NDArray *third, const std::function<bfloat16 (bfloat16, bfloat16, bfloat16)>& func, NDArray* target);
    template void NDArray::applyTriplewiseLambda(NDArray* second, NDArray *third, const std::function<Nd4jLong (Nd4jLong, Nd4jLong, Nd4jLong)>& func, NDArray* target);
    template void NDArray::applyTriplewiseLambda(NDArray* second, NDArray *third, const std::function<int (int, int, int)>& func, NDArray* target);
    template void NDArray::applyTriplewiseLambda(NDArray* second, NDArray *third, const std::function<int16_t (int16_t, int16_t, int16_t)>& func, NDArray* target);
    template void NDArray::applyTriplewiseLambda(NDArray* second, NDArray *third, const std::function<uint8_t (uint8_t, uint8_t, uint8_t)>& func, NDArray* target);
    template void NDArray::applyTriplewiseLambda(NDArray* second, NDArray *third, const std::function<int8_t (int8_t, int8_t, int8_t)>& func, NDArray* target);
    template void NDArray::applyTriplewiseLambda(NDArray* second, NDArray *third, const std::function<bool (bool, bool, bool)>& func, NDArray* target);


    template<typename T>
    void NDArray::applyPairwiseLambda(const NDArray* other, const std::function<T(T, T)>& func, NDArray* target) {
        if (target == nullptr)
            target = this;

        if (other == nullptr) {
            nd4j_printf("applyPairwiseLambda requires both operands to be valid NDArrays, but Y is NULL\n","");
            throw std::runtime_error("Other is null");
        }

        if(_dataType != DataTypeUtils::fromT<T>())
            throw std::runtime_error("NDArray::applyPairwiseLambda<T> method: wrong template parameter T, its type should be the same as type of this array!");
        if(_dataType != other->_dataType || _dataType != target->_dataType)
            throw std::runtime_error("NDArray::applyPairwiseLambda<T> method: all three arrays (this, other, target) must have the same type !");

        if (this->lengthOf() != other->lengthOf()) {
            nd4j_printf("applyPairwiseLambda requires both operands to have the same shape\n","");
            throw std::runtime_error("Shapes mismach");
        }

        auto f = this->bufferAsT<T>();
        auto s = other->bufferAsT<T>();
        auto z = target->bufferAsT<T>();

        if (this->ordering() == other->ordering() && this->ordering() == target->ordering() && (this->ews() == 1 && target->ews() == 1) && this->ews() == other->ews()) {

            PRAGMA_OMP_PARALLEL_FOR_SIMD
            for (int e = 0; e < _length; e++)
                z[e] = func(f[e], s[e]);
        } else {
            if (f == z) {

                PRAGMA_OMP_PARALLEL_FOR_SIMD
                for (int e = 0; e < _length; e++) {

                    auto xOffset = this->getOffset(e);
                    auto yOffset = other->getOffset(e);

                    f[xOffset] = func(f[xOffset], s[yOffset]);
                }
            } else {

                PRAGMA_OMP_PARALLEL_FOR_SIMD
                for (int e = 0; e < _length; e++) {

                    auto xOffset = this->getOffset(e);
                    auto yOffset = other->getOffset(e);
                    auto zOffset = target->getOffset(e);

                    z[zOffset] = func(f[xOffset], s[yOffset]);
                }
            }
        }
    }
    template void NDArray::applyPairwiseLambda(const NDArray* other, const std::function<double (double, double)>& func, NDArray* target);
    template void NDArray::applyPairwiseLambda(const NDArray* other, const std::function<float (float, float)>& func, NDArray* target);
    template void NDArray::applyPairwiseLambda(const NDArray* other, const std::function<float16 (float16, float16)>& func, NDArray* target);
    template void NDArray::applyPairwiseLambda(const NDArray* other, const std::function<bfloat16 (bfloat16, bfloat16)>& func, NDArray* target);
    template void NDArray::applyPairwiseLambda(const NDArray* other, const std::function<Nd4jLong (Nd4jLong, Nd4jLong)>& func, NDArray* target);
    template void NDArray::applyPairwiseLambda(const NDArray* other, const std::function<int (int, int)>& func, NDArray* target);
    template void NDArray::applyPairwiseLambda(const NDArray* other, const std::function<int16_t (int16_t, int16_t)>& func, NDArray* target);
    template void NDArray::applyPairwiseLambda(const NDArray* other, const std::function<uint8_t (uint8_t, uint8_t)>& func, NDArray* target);
    template void NDArray::applyPairwiseLambda(const NDArray* other, const std::function<int8_t (int8_t, int8_t)>& func, NDArray* target);
    template void NDArray::applyPairwiseLambda(const NDArray* other, const std::function<bool (bool, bool)>& func, NDArray* target);


////////////////////////////////////////////////////////////////////////
    template<typename T>
    void NDArray::applyLambda(const std::function<T(T)>& func, NDArray* target) {
        if (target == nullptr)
            target = this;

        if(_dataType != DataTypeUtils::fromT<T>())
            throw std::runtime_error("NDArray::applyLambda<T> method: wrong template parameter T, its type should be the same as type of this array!");
        if(_dataType != target->_dataType)
            throw std::runtime_error("NDArray::applyLambda<T> method: types of this and target array should match !");

        auto f = this->bufferAsT<T>();
        auto z = target->bufferAsT<T>();

        if (this->ordering() == target->ordering() && (this->ews() == 1 && target->ews() == 1)) {

            PRAGMA_OMP_PARALLEL_FOR_SIMD
            for (int e = 0; e < _length; e++)
                z[e] = func(f[e]);
        } else {
            if (f == z) {

                PRAGMA_OMP_PARALLEL_FOR_SIMD
                for (int e = 0; e < _length; e++) {

                    auto xOffset = this->getOffset(e);

                    f[xOffset] = func(f[xOffset]);
                }
            } else {

                PRAGMA_OMP_PARALLEL_FOR_SIMD
                for (int e = 0; e < _length; e++) {

                    auto xOffset = this->getOffset(e);
                    auto zOffset = target->getOffset(e);

                    z[zOffset] = func(f[xOffset]);
                }
            }
        }
    }
    template void NDArray::applyLambda(const std::function<double(double)>& func, NDArray* target);
    template void NDArray::applyLambda(const std::function<float(float)>& func, NDArray* target);
    template void NDArray::applyLambda(const std::function<float16(float16)>& func, NDArray* target);
    template void NDArray::applyLambda(const std::function<bfloat16(bfloat16)>& func, NDArray* target);
    template void NDArray::applyLambda(const std::function<Nd4jLong(Nd4jLong)>& func, NDArray* target);
    template void NDArray::applyLambda(const std::function<int16_t(int16_t)>& func, NDArray* target);
    template void NDArray::applyLambda(const std::function<int32_t(int32_t)>& func, NDArray* target);
    template void NDArray::applyLambda(const std::function<uint8_t(uint8_t)>& func, NDArray* target);
    template void NDArray::applyLambda(const std::function<int8_t(int8_t)>& func, NDArray* target);
    template void NDArray::applyLambda(const std::function<bool(bool)>& func, NDArray* target);

    template<typename T>
    void NDArray::applyIndexedLambda(const std::function<T(Nd4jLong, T)>& func, NDArray* target) {
        if (target == nullptr)
            target = this;

        if(_dataType != DataTypeUtils::fromT<T>())
            throw std::runtime_error("NDArray::applyIndexedLambda<T> method: wrong template parameter T, its type should be the same as type of this array!");
        if(_dataType != target->_dataType)
            throw std::runtime_error("NDArray::applyIndexedLambda<T> method: types of this and target array should match !");

        auto f = this->bufferAsT<T>();
        auto z = target->bufferAsT<T>();

        if (this->ordering() == target->ordering() && (this->ews() == 1 && target->ews() == 1)) {

            PRAGMA_OMP_PARALLEL_FOR_SIMD
            for (Nd4jLong e = 0; e < _length; e++)
                z[e] = func(e, f[e]);
        } else {
            if (f == z) {

                PRAGMA_OMP_PARALLEL_FOR_SIMD
                for (Nd4jLong e = 0; e < _length; e++) {

                    auto xOffset = this->getOffset(e);

                    f[xOffset] = func(e, f[xOffset]);
                }
            } else {

                PRAGMA_OMP_PARALLEL_FOR_SIMD
                for (Nd4jLong e = 0; e < _length; e++) {

                    auto xOffset = this->getOffset(e);
                    auto zOffset = target->getOffset(e);

                    z[zOffset] = func(e, f[xOffset]);
                }
            }
        }
    }
    template void NDArray::applyIndexedLambda(const std::function<double(Nd4jLong, double)>& func, NDArray* target);
    template void NDArray::applyIndexedLambda(const std::function<float(Nd4jLong, float)>& func, NDArray* target);
    template void NDArray::applyIndexedLambda(const std::function<float16(Nd4jLong, float16)>& func, NDArray* target);
    template void NDArray::applyIndexedLambda(const std::function<bfloat16(Nd4jLong, bfloat16)>& func, NDArray* target);
    template void NDArray::applyIndexedLambda(const std::function<Nd4jLong(Nd4jLong, Nd4jLong)>& func, NDArray* target);
    template void NDArray::applyIndexedLambda(const std::function<int(Nd4jLong, int)>& func, NDArray* target);
    template void NDArray::applyIndexedLambda(const std::function<int16_t(Nd4jLong, int16_t)>& func, NDArray* target);
    template void NDArray::applyIndexedLambda(const std::function<uint8_t (Nd4jLong, uint8_t)>& func, NDArray* target);
    template void NDArray::applyIndexedLambda(const std::function<int8_t(Nd4jLong, int8_t)>& func, NDArray* target);
    template void NDArray::applyIndexedLambda(const std::function<bool(Nd4jLong, bool)>& func, NDArray* target);


    template<typename T>
    void NDArray::applyIndexedPairwiseLambda(NDArray* other, const std::function<T(Nd4jLong, T, T)>& func, NDArray* target) {
        if (target == nullptr)
            target = this;

        if (other == nullptr) {
            nd4j_printf("applyIndexedPairwiseLambda requires both operands to be valid NDArrays, but Y is NULL\n","");
            throw std::runtime_error("Other is null");
        }
        if(_dataType != DataTypeUtils::fromT<T>())
            throw std::runtime_error("NDArray::applyIndexedPairwiseLambda<T> method: wrong template parameter T, its type should be the same as type of this array!");
        if(_dataType != target->_dataType)
            throw std::runtime_error("NDArray::applyIndexedPairwiseLambda<T> method: types of this and target array should match !");
        if (this->lengthOf() != other->lengthOf()) {
            nd4j_printf("applyIndexedPairwiseLambda requires both operands to have the same shape\n","");
            throw std::runtime_error("Shapes mismach");
        }

        auto f = this->bufferAsT<T>();
        auto s = other->bufferAsT<T>();
        auto z = target->bufferAsT<T>();

        if (this->ordering() == other->ordering() && this->ordering() == target->ordering() && (this->ews() == 1 && target->ews() == 1) && this->ews() == other->ews()) {

            PRAGMA_OMP_PARALLEL_FOR_SIMD
            for (Nd4jLong e = 0; e < _length; e++)
                z[e] = func((Nd4jLong) e, f[e], s[e]);
        } else {
            if (f == z) {

                PRAGMA_OMP_PARALLEL_FOR_SIMD
                for (int e = 0; e < _length; e++) {

                    auto xOffset = this->getOffset(e);
                    auto yOffset = other->getOffset(e);

                    f[xOffset] = func((Nd4jLong) e, f[xOffset], s[yOffset]);
                }
            } else {

                PRAGMA_OMP_PARALLEL_FOR_SIMD
                for (int e = 0; e < _length; e++) {

                    auto xOffset = this->getOffset(e);
                    auto yOffset = other->getOffset(e);
                    auto zOffset = target->getOffset(e);

                    z[zOffset] = func((Nd4jLong) e, f[xOffset], s[yOffset]);
                }
            }
        }
    }
    template void NDArray::applyIndexedPairwiseLambda(NDArray* other, const std::function<double (Nd4jLong, double, double)>& func, NDArray* target);
    template void NDArray::applyIndexedPairwiseLambda(NDArray* other, const std::function<float (Nd4jLong, float, float)>& func, NDArray* target);
    template void NDArray::applyIndexedPairwiseLambda(NDArray* other, const std::function<float16 (Nd4jLong, float16, float16)>& func, NDArray* target);
    template void NDArray::applyIndexedPairwiseLambda(NDArray* other, const std::function<bfloat16 (Nd4jLong, bfloat16, bfloat16)>& func, NDArray* target);
    template void NDArray::applyIndexedPairwiseLambda(NDArray* other, const std::function<Nd4jLong (Nd4jLong, Nd4jLong, Nd4jLong)>& func, NDArray* target);
    template void NDArray::applyIndexedPairwiseLambda(NDArray* other, const std::function<int (Nd4jLong, int, int)>& func, NDArray* target);
    template void NDArray::applyIndexedPairwiseLambda(NDArray* other, const std::function<int16_t (Nd4jLong, int16_t, int16_t)>& func, NDArray* target);
    template void NDArray::applyIndexedPairwiseLambda(NDArray* other, const std::function<uint8_t (Nd4jLong, uint8_t, uint8_t)>& func, NDArray* target);
    template void NDArray::applyIndexedPairwiseLambda(NDArray* other, const std::function<int8_t (Nd4jLong, int8_t, int8_t)>& func, NDArray* target);
    template void NDArray::applyIndexedPairwiseLambda(NDArray* other, const std::function<bool (Nd4jLong, bool, bool)>& func, NDArray* target);
#endif

//////////////////////////////////////////////////////////////////////////
// perform array transformation
    void NDArray::applyTransform(nd4j::transform::FloatOps op, NDArray *target, ExtraArguments *extraParams) {

        if (isS())
            throw std::runtime_error("NDArray::applyTransform FloatOps: you can't use this method on String array!");

        if (target == nullptr)
            target = this;

        if (!target->isR())
            throw std::runtime_error("NDArray::applyTransform FloatOps: target array must have one of FLOAT types");

        NativeOpExecutioner::execTransformFloat(_context, op, _buffer, _shapeInfo, _bufferD, _shapeInfoD, target->_buffer, target->_shapeInfo, target->_bufferD, target->_shapeInfoD, extraParams != nullptr ? extraParams->argumentsAsT(target->dataType()) : nullptr, nullptr, nullptr);
    }

    void NDArray::applyTransform(nd4j::transform::AnyOps op, NDArray *target, ExtraArguments *extraParams) {
        if (isS())
            throw std::runtime_error("NDArray::applyTransform FloatOps: you can't use this method on String array!");

        if (target == nullptr)
            target = this;

        NativeOpExecutioner::execTransformAny(_context, op, _buffer, _shapeInfo, _bufferD, _shapeInfoD, target->_buffer, target->_shapeInfo, target->_bufferD, target->_shapeInfoD, extraParams != nullptr ? extraParams->argumentsAsT(target->dataType()) : nullptr, nullptr, nullptr);
    }

    void NDArray::applyTransform(nd4j::transform::SameOps op, NDArray *target, ExtraArguments *extraParams) {
        //nd4j_printf("Same op %i transform:\n", (int)op);
        if (isS())
            throw std::runtime_error("NDArray::applyTransform SameOps: you can't use this method on String array!");

        if (target == nullptr)
            target = this;

        if (target->dataType() != this->dataType())
            throw std::runtime_error("NDArray::applyTransform SameOps: target array must have the same data type as original array");
        NDArray::registerSpecialUse({target}, {this});
        NativeOpExecutioner::execTransformSame(_context, op, this->_buffer, this->_shapeInfo, this->_bufferD, this->_shapeInfoD, target->_buffer, target->_shapeInfo, target->_bufferD, target->_shapeInfoD, extraParams != nullptr ? extraParams->argumentsAsT(target->dataType()) : nullptr, nullptr, nullptr);
    }

    void NDArray::applyTransform(nd4j::transform::BoolOps op, NDArray *target, ExtraArguments *extraParams) {
        if (isS())
            throw std::runtime_error("NDArray::applyTransform BoolOps: you can't use this method on String array!");

        if (target == nullptr)
            target = this;

        if (!target->isB())
            throw std::runtime_error("NDArray::applyTransform BoolOps: target array must have one of BOOL types");

        NDArray::registerSpecialUse({target}, {this});
        NativeOpExecutioner::execTransformBool(_context, op, this->_buffer, this->_shapeInfo, _bufferD, _shapeInfoD, target->_buffer, target->_shapeInfo, target->_bufferD, target->_shapeInfoD, extraParams != nullptr ? extraParams->argumentsAsT(this->dataType()) : nullptr, nullptr, nullptr);
    }

    void NDArray::applyTransform(nd4j::transform::StrictOps op, NDArray *target, ExtraArguments *extraParams) {
        if (isS())
            throw std::runtime_error("NDArray::applyTransform StrictOps: you can't use this method on String array!");

            if (target == nullptr)
                target = this;

        if (!this->isR() || !target->isR() || (this->dataType() != target->dataType()))
            throw std::runtime_error("NDArray::applyTransform StrictOps: both Source and Target array must have same FLOAT type !");

        NDArray::registerSpecialUse({target}, {this});
        NativeOpExecutioner::execTransformStrict(_context, op, this->_buffer, this->_shapeInfo, _bufferD, _shapeInfoD, target->_buffer, target->_shapeInfo, target->_bufferD, target->_shapeInfoD, extraParams != nullptr ? extraParams->argumentsAsT(target->dataType()) : nullptr, nullptr, nullptr);
    }

    // perform array transformation
    NDArray NDArray::transform(nd4j::transform::FloatOps op, void *extraParams) const {
        if (isS())
            throw std::runtime_error("NDArray::transform FloatOps: you can't use this method on String array!");

        NDArray result(this->ordering(), getShapeAsVector(), DataTypeUtils::pickFloatingType(dataType()), this->_context);
        NativeOpExecutioner::execTransformFloat(_context, op, this->_buffer, this->_shapeInfo, this->_bufferD, this->_shapeInfoD, result._buffer, result._shapeInfo, result._bufferD, result._shapeInfoD, extraParams, nullptr, nullptr);
        return result;
    }

    NDArray NDArray::transform(nd4j::transform::SameOps op, void *extraParams) const {
        if (isS())
            throw std::runtime_error("NDArray::transform SameOps: you can't use this method on String array!");

        NDArray result(this->_shapeInfo, false, this->_context);
        NativeOpExecutioner::execTransformSame(_context, op, this->_buffer, this->_shapeInfo, this->_bufferD, this->_shapeInfoD, result._buffer, result._shapeInfo, result._bufferD, result._shapeInfoD, extraParams, nullptr, nullptr);
        return result;
    }

    NDArray NDArray::transform(nd4j::transform::StrictOps op, void *extraParams) const {
        if (!this->isR())
            throw std::runtime_error("Source array must have one of FLOAT types");

        NDArray result(this->_shapeInfo, false, this->_context);
        NativeOpExecutioner::execTransformStrict(_context, op, this->_buffer, this->_shapeInfo, this->_bufferD, this->_shapeInfoD, result._buffer, result._shapeInfo, result._bufferD, result._shapeInfoD, extraParams, nullptr, nullptr);
        return result;
    }

    NDArray NDArray::transform(nd4j::transform::BoolOps op, void *extraParams) const {
        if (isS())
            throw std::runtime_error("NDArray::transform BoolOps: you can't use this method on String array!");

        NDArray result(this->ordering(), getShapeAsVector(), nd4j::DataType::BOOL, this->_context);
        NativeOpExecutioner::execTransformBool(_context, op, this->_buffer, this->_shapeInfo, this->_bufferD, this->_shapeInfoD, result._buffer, result._shapeInfo, result._bufferD, result._shapeInfoD, extraParams, nullptr, nullptr);
        return result;

    }


//////////////////////////////////////////////////////////////////////////
    void NDArray::applyScalarArr(nd4j::scalar::BoolOps op, const NDArray* scalar, NDArray *target, ExtraArguments *extraParams) const {
        if (isS())
            throw std::runtime_error("NDArray::applyScalarArr BoolOps: you can't use this method on String array!");
        if (target == nullptr || !target->isB())
            throw std::invalid_argument("NDArray::applyScalarArr bool method: target is nullptr or has not bool type!");
        if (_dataType != scalar->_dataType) {
            nd4j_printf("This dtype: [%i]; scalar dtype: [%i]\n", this->_dataType, scalar->_dataType);
            throw std::invalid_argument("NDArray::applyScalarArr bool method: this and scalar arrays must have the same type!");
        }
        NativeOpExecutioner::execScalarBool(_context, op, _buffer, _shapeInfo, _bufferD, _shapeInfoD, target->_buffer, target->_shapeInfo, target->_bufferD, target->_shapeInfoD, scalar->_buffer, scalar->_shapeInfo, scalar->_bufferD, scalar->_shapeInfoD, extraParams != nullptr ? extraParams->argumentsAsT(target->dataType()): nullptr);
    }

    template <typename T>
    void NDArray::applyScalar(nd4j::scalar::BoolOps op, const T scalar, NDArray *target, ExtraArguments *extraParams) const {

        auto scalarArr = NDArrayFactory::create<T>(scalar, _context);
        applyScalarArr(op, &scalarArr, target, extraParams);
    }

    template <> void NDArray::applyScalar(nd4j::scalar::BoolOps op, const NDArray* scalar, NDArray *target, ExtraArguments *extraParams) const { throw std::runtime_error("NDArray::applyScalar<NDArray*> method: do not use me!");}
    template void NDArray::applyScalar<double>(nd4j::scalar::BoolOps op, const double scalar, NDArray *target, ExtraArguments *extraParams) const;
    template void NDArray::applyScalar<float>(nd4j::scalar::BoolOps op, const float scalar, NDArray *target, ExtraArguments *extraParams) const;
    template void NDArray::applyScalar<float16>(nd4j::scalar::BoolOps op, const float16 scalar, NDArray *target, ExtraArguments *extraParams) const;
    template void NDArray::applyScalar<bfloat16>(nd4j::scalar::BoolOps op, const bfloat16 scalar, NDArray *target, ExtraArguments *extraParams) const;
    template void NDArray::applyScalar<Nd4jLong>(nd4j::scalar::BoolOps op, const Nd4jLong scalar, NDArray *target, ExtraArguments *extraParams) const;
    template void NDArray::applyScalar<int>(nd4j::scalar::BoolOps op, const int scalar, NDArray *target, ExtraArguments *extraParams) const;
    template void NDArray::applyScalar<int16_t>(nd4j::scalar::BoolOps op, const int16_t scalar, NDArray *target, ExtraArguments *extraParams) const;
    template void NDArray::applyScalar<int8_t>(nd4j::scalar::BoolOps op, const int8_t scalar, NDArray *target, ExtraArguments *extraParams) const;
    template void NDArray::applyScalar<uint8_t>(nd4j::scalar::BoolOps op, const uint8_t scalar, NDArray *target, ExtraArguments *extraParams) const;
    template void NDArray::applyScalar<bool>(nd4j::scalar::BoolOps op, const bool scalar, NDArray *target, ExtraArguments *extraParams) const;

//////////////////////////////////////////////////////////////////////////
    void NDArray::applyScalarArr(nd4j::scalar::Ops op, const NDArray* scalar, NDArray* target, ExtraArguments *extraParams) {
        if (isS())
            throw std::runtime_error("NDArray::applyScalarArr: you can't use this method on String array!");
        if (!scalar->isScalar())
            throw std::invalid_argument("NDArray::applyScalarArr method: operand is not a scalar!");
        if(target == nullptr)
            target = this;
        if(target->_dataType != DataTypeUtils::pickPairwiseResultType(_shapeInfo, scalar->_shapeInfo) && !(target->_dataType == this->_dataType || target->_dataType == scalar->_dataType))
            throw std::invalid_argument("NDArray::applyScalarArr method: wrong type of target array!");

        if (this->dataType() != scalar->dataType()) {
            auto tmp = scalar->cast(this->dataType());

            NativeOpExecutioner::execScalar(_context, op, _buffer, _shapeInfo, _bufferD, _shapeInfoD, target->_buffer, target->_shapeInfo, target->_bufferD, target->_shapeInfoD, tmp->getBuffer(), tmp->getShapeInfo(), tmp->_bufferD, tmp->_shapeInfoD, extraParams != nullptr ? extraParams->argumentsAsT(target->dataType()) : nullptr);

            delete tmp;
        } else {
            NativeOpExecutioner::execScalar(_context, op, _buffer, _shapeInfo, _bufferD, _shapeInfoD, target->_buffer, target->_shapeInfo, target->_bufferD, target->_shapeInfoD, scalar->getBuffer(), scalar->getShapeInfo(), scalar->_bufferD, scalar->_shapeInfoD, extraParams != nullptr ? extraParams->argumentsAsT(target->dataType()) : nullptr);
        }
    }

    template <typename T>
    void NDArray::applyScalar(nd4j::scalar::Ops op, const T scalar, NDArray *target, ExtraArguments *extraParams) {

        auto scalarArr = NDArrayFactory::create<T>(this->dataType(), scalar, this->_context);
        applyScalarArr(op, &scalarArr, target, extraParams);
    }

    template <> void NDArray::applyScalar(nd4j::scalar::Ops op, const NDArray* scalar, NDArray *target, ExtraArguments *extraParams) { throw std::runtime_error("NDArray::applyScalar<NDArray*> method: do not use me!");}
    template void NDArray::applyScalar(nd4j::scalar::Ops op, const double scalar, NDArray *target, ExtraArguments *extraParams);
    template void NDArray::applyScalar(nd4j::scalar::Ops op, const float scalar, NDArray *target, ExtraArguments *extraParams);
    template void NDArray::applyScalar(nd4j::scalar::Ops op, const float16 scalar, NDArray *target, ExtraArguments *extraParams);
    template void NDArray::applyScalar(nd4j::scalar::Ops op, const bfloat16 scalar, NDArray *target, ExtraArguments *extraParams);
    template void NDArray::applyScalar(nd4j::scalar::Ops op, const Nd4jLong scalar, NDArray *target, ExtraArguments *extraParams);
    template void NDArray::applyScalar(nd4j::scalar::Ops op, const int scalar, NDArray *target, ExtraArguments *extraParams);
    template void NDArray::applyScalar(nd4j::scalar::Ops op, const int16_t scalar, NDArray *target, ExtraArguments *extraParams);
    template void NDArray::applyScalar(nd4j::scalar::Ops op, const int8_t scalar, NDArray *target, ExtraArguments *extraParams);
    template void NDArray::applyScalar(nd4j::scalar::Ops op, const uint8_t scalar, NDArray *target, ExtraArguments *extraParams);
    template void NDArray::applyScalar(nd4j::scalar::Ops op, const bool scalar, NDArray *target, ExtraArguments *extraParams);

    //////////////////////////////////////////////////////////////////////////
    void NDArray::applyBroadcast(nd4j::broadcast::Ops op, const std::vector<int>& dimensions, const NDArray* tadArray, NDArray* target, ExtraArguments* extraArgs) {
        if (isS())
            throw std::runtime_error("NDArray::applyBroadcast: you can't use this method on String array!");
        if(((op == broadcast::Divide || op == broadcast::FloorDiv || op == broadcast::FloorMod) && tadArray->isB()) || (op == broadcast::ReverseDivide && this->isB()))
            throw std::runtime_error("NDArray::applyBroadcast: you can't divide by array!");

        if (dimensions.size() == 0)
            return;
        auto result = target == nullptr ? this : target;

        if(result->_dataType != DataTypeUtils::pickPairwiseResultType(_shapeInfo, tadArray->_shapeInfo))
            throw std::invalid_argument("NDArray::applyBroadcast method: wrong type of target array !");
        if(!result->isSameShape(this))
            throw std::invalid_argument("NDArray::applyBroadcast method: this and target arrays must have the same shape !");

        auto pack = ConstantTadHelper::getInstance()->tadForDimensions(_shapeInfo, dimensions);
        auto packZ = ConstantTadHelper::getInstance()->tadForDimensions(result->_shapeInfo, dimensions);

        auto tadLength = shape::length(pack.primaryShapeInfo());
        if (tadLength != tadArray->lengthOf())
            throw std::runtime_error("NDArray::applyBroadcast method: tad length mismatch !");

        NativeOpExecutioner::execBroadcast(_context, op, this->_buffer, this->_shapeInfo, this->_bufferD, this->_shapeInfoD, tadArray->_buffer, tadArray->_shapeInfo, tadArray->_bufferD, tadArray->_shapeInfoD, result->_buffer, result->_shapeInfo, result->_bufferD, result->_shapeInfoD, const_cast<int*>(dimensions.data()), (int)dimensions.size(), pack.primaryShapeInfo(), pack.primaryOffsets(), packZ.primaryShapeInfo(), packZ.primaryOffsets());
    }

    //////////////////////////////////////////////////////////////////////////
    void NDArray::applyBroadcast(nd4j::broadcast::BoolOps op, const std::vector<int>& dimensions, const NDArray* tadArray, NDArray* target, ExtraArguments* extraArgs) {
        if (isS())
            throw std::runtime_error("NDArray::applyBroadcast BoolOps: you can't use this method on String array!");

        if (dimensions.size() == 0)
            return;

        auto result = target == nullptr ? this : target;

        if(result->_dataType != DataType::BOOL)
            throw std::invalid_argument("NDArray::applyBroadcast bool method: type of target array must be BOOL!");
        if(!result->isSameShape(this))
            throw std::invalid_argument("NDArray::applyBroadcast bool method: this and other arrays must have the same shape !");
        if(_dataType != tadArray->_dataType)
            throw std::invalid_argument("NDArray::applyBroadcast bool method: this and tad arrays must have the same type !");

        auto pack = ConstantTadHelper::getInstance()->tadForDimensions(_shapeInfo, dimensions);
        auto packZ = ConstantTadHelper::getInstance()->tadForDimensions(result->_shapeInfo, dimensions);

        auto tadLength = shape::length(pack.primaryShapeInfo());
        if (tadLength != tadArray->lengthOf())
            throw std::runtime_error("NDArray::applyBroadcast bool method: tad length mismatch !");

        NativeOpExecutioner::execBroadcastBool(_context, op, this->_buffer, this->_shapeInfo, this->_bufferD, this->_shapeInfoD,
                                               tadArray->_buffer, tadArray->_shapeInfo, tadArray->_bufferD, tadArray->_shapeInfoD,
                                               result->_buffer, result->_shapeInfo, result->_bufferD, result->_shapeInfoD,
                                               const_cast<int*>(dimensions.data()), (int)dimensions.size(), pack.primaryShapeInfo(), pack.primaryOffsets(), packZ.primaryShapeInfo(), packZ.primaryOffsets());
    }

//////////////////////////////////////////////////////////////////////////
    NDArray NDArray::applyTrueBroadcast(nd4j::BroadcastOpsTuple op, const NDArray& other, ExtraArguments *extraArgs) const {
        Nd4jLong* newShapeInfo = nullptr;
        if(!ShapeUtils::evalBroadcastShapeInfo(*this, other, true, newShapeInfo, _context->getWorkspace()))          // the rank of new array = max->rankOf)()
            throw std::runtime_error("NDArray::applyTrueBroadcast method: the shapes of this and other arrays are not suitable for broadcast operation !");
        NDArray result(newShapeInfo, true, this->_context);

        this->applyTrueBroadcast(op, &other, &result, false, extraArgs);

        return result;
    }
    void NDArray::applyIndexReduce(nd4j::indexreduce::Ops op, NDArray* target, const std::vector<int>& dimensions, const ExtraArguments *extraParams) const {
        if (isS())
            throw std::runtime_error("NDArray::applyIndexReduce: you can't use this method on String array!");

        if (target->dataType() != nd4j::DataType::INT64)
            throw std::runtime_error("IndexReduce operations return INT64");

        if (target->isScalar()) {
            //target->_buffer[0] = functions::indexreduce::IndexReduce<T>::template execScalar<OpName>(_buffer, _shapeInfo, const_cast<T*>(extraParams));
            NativeOpExecutioner::execIndexReduceScalar(_context, op, _buffer, _shapeInfo, _bufferD, _shapeInfoD, extraParams != nullptr ? const_cast<ExtraArguments*>(extraParams)->argumentsAsT(this->dataType()) : nullptr, target->getBuffer(), target->getShapeInfo(), target->getSpecialBuffer(), target->getSpecialShapeInfo());
        } else {
            auto pack = ConstantTadHelper::getInstance()->tadForDimensions(_shapeInfo, dimensions);

            NativeOpExecutioner::execIndexReduce(_context, op, _buffer, _shapeInfo, _bufferD, _shapeInfoD, extraParams != nullptr ? const_cast<ExtraArguments*>(extraParams)->argumentsAsT(this->dataType()) : nullptr,
                                                 reinterpret_cast<Nd4jLong *>(target->_buffer),
                                                 target->_shapeInfo, target->_bufferD, target->_shapeInfoD,
                                                 const_cast<int*>(dimensions.data()), dimensions.size(),
                                                 pack.primaryShapeInfo(), pack.primaryOffsets());
        }
    }
    ////////////////////////////////////////////////////////////////////////
    // reduce dimensions in this array relying on index operations
    NDArray* NDArray::applyIndexReduce(nd4j::indexreduce::Ops op,const std::vector<int>& dimensions, const ExtraArguments* extraParams ) const {
        if (isS())
            throw std::runtime_error("NDArray::applyIndexReduce: you can't use this method on String array!");

        auto newShape = ShapeUtils::evalReduceShapeInfo('c', const_cast<std::vector<int>&>(dimensions), *this, DataType::INT64, false, false, _context->getWorkspace());
        auto result = new NDArray(newShape, true, _context);

        if (rankOf() == dimensions.size()) {
            NativeOpExecutioner::execIndexReduceScalar(_context, op, _buffer, _shapeInfo, _bufferD, _shapeInfoD, extraParams != nullptr ? const_cast<ExtraArguments*>(extraParams)->argumentsAsT(this->dataType()) : nullptr, result->getBuffer(), result->getShapeInfo(), result->getSpecialBuffer(), result->getSpecialShapeInfo());
        } else {
            auto pack = ConstantTadHelper::getInstance()->tadForDimensions(_shapeInfo, dimensions);

            NativeOpExecutioner::execIndexReduce(_context, op, _buffer, _shapeInfo, _bufferD, _shapeInfoD, extraParams != nullptr ? const_cast<ExtraArguments*>(extraParams)->argumentsAsT(this->dataType()) : nullptr,
                                                 reinterpret_cast<Nd4jLong *>(result->_buffer),
                                                 result->_shapeInfo, result->_bufferD, result->_shapeInfoD,
                                                 const_cast<int*>(dimensions.data()), dimensions.size(),
                                                 pack.primaryShapeInfo(), pack.primaryOffsets());
        }

        return result;
    }

    ////////////////////////////////////////////////////////////////////////
    // apply reduce3 operations to this and other array, return result in new output array
    NDArray* NDArray::applyReduce3(nd4j::reduce3::Ops op, const NDArray* other, const ExtraArguments* extraParams) const {
        if (isS())
            throw std::runtime_error("NDArray::applyReduce3 method: you can't use this method on String array!");
        if(_dataType != other->_dataType)
            throw std::runtime_error("NDArray::applyReduce3 method: the types of this and other arrays must be the same !");
        // check shapes consistency
        if(!isSameShape(other))
            throw std::runtime_error("NDArray::applyReduce3 method: the shapes of this and other arrays must be the same !");
        // create shapeInfo for scalar
        auto newShape = ConstantShapeHelper::getInstance()->scalarShapeInfo(DataTypeUtils::pickFloatingType(_dataType));
        // create output array (scalar)
        auto result = new NDArray(newShape, true, _context);
        // create dynamic array of extra parameters if array extraParams is empty (==nullptr)
        void* params = extraParams != nullptr ? const_cast<ExtraArguments*>(extraParams)->argumentsAsT(this->dataType()) : nullptr;
        if(params == nullptr) {
            params = new int8_t[result->sizeOfT()*3];
            memset(params, 0, result->sizeOfT()*3);
        }
        NativeOpExecutioner::execReduce3Scalar(_context, op, _buffer, _shapeInfo, _bufferD, _shapeInfoD, params, other->_buffer, other->_shapeInfo, other->_bufferD, other->_shapeInfoD, result->_buffer, result->_shapeInfo, result->_bufferD, result->_shapeInfoD);

        if(params != extraParams)
            delete [] static_cast<int8_t*>(params);

        return result;
    }

    ////////////////////////////////////////////////////////////////////////
    // apply reduce3 (execAll) operations to this and other array, return result in new output array
    NDArray* NDArray::applyAllReduce3(nd4j::reduce3::Ops op, const NDArray *other, const std::vector<int>& dimensions, const ExtraArguments* extraParams) const {
        if (isS())
            throw std::runtime_error("NDArray::applyAllReduce3: you can't use this method on String array!");
        if(_dataType != other->_dataType)
            throw std::runtime_error("NDArray::applyAllReduce3 method: the types of this and other arrays must be the same !");
        // be careful, copy array may undergo changes (sort, transformation of negative dimensions to positive, duplicates removing )
        std::vector<int> copy(dimensions);
        shape::checkDimensions(rankOf(), copy);
        shape::checkDimensions(other->rankOf(), copy);

        auto packX = ConstantTadHelper::getInstance()->tadForDimensions(_shapeInfo, copy);
        auto packY = ConstantTadHelper::getInstance()->tadForDimensions(other->_shapeInfo, copy);

        // check tads shapes
        if(!shape::equalsSoft(packX.primaryShapeInfo(), packY.primaryShapeInfo()))
            throw std::runtime_error("NDArray::applyAllReduce3 method: the shapes of array tads are different !");

        // set newShape for output array
        auto newShape = ConstantShapeHelper::getInstance()->createShapeInfo(DataTypeUtils::pickFloatingType(_dataType), 'c', {packX.numberOfTads(), packY.numberOfTads()});

        // create output array
        auto result = new NDArray(newShape, true, _context);
        // create dynamic array of extra parameters if array extraParams is empty (==nullptr)
        void* params = extraParams != nullptr ? const_cast<ExtraArguments*>(extraParams)->argumentsAsT(this->dataType()) : nullptr;
        if(params == nullptr) {
            params = new int8_t[result->sizeOfT()*3];
            memset(params, 0, result->sizeOfT()*3);
        }

        NativeOpExecutioner::execReduce3All(_context, op, _buffer, _shapeInfo, _bufferD, _shapeInfoD, params,
                                            other->_buffer, other->_shapeInfo, other->_bufferD, other->_shapeInfoD,
                                            result->_buffer,result->_shapeInfo, result->_bufferD, result->_shapeInfoD,
                                            copy.data(), copy.size(), packX.primaryShapeInfo(), packX.primaryOffsets(), packY.primaryShapeInfo(), packY.primaryOffsets());
        if(params != extraParams)
            delete [] static_cast<int8_t*>(params);

        return result;
    }

    ////////////////////////////////////////////////////////////////////////
    // apply reduce3 (exec) operations to this and other array, return result in new output array
    NDArray* NDArray::applyReduce3(nd4j::reduce3::Ops op, const NDArray* other, const std::vector<int>& dimensions, const ExtraArguments* extraParams) const {
        if (isS())
            throw std::runtime_error("NDArray::applyReduce3: you can't use this method on String array!");
        if(_dataType != other->_dataType)
            throw std::runtime_error("NDArray::applyReduce3 method: the types of this and other arrays must be the same !");

        std::vector<int> copy(dimensions);
        shape::checkDimensions(rankOf(), copy);
        shape::checkDimensions(other->rankOf(), copy);

        auto newShape = ShapeUtils::evalReduceShapeInfo('c', copy, *this, DataTypeUtils::pickFloatingType(_dataType), false, false, _context->getWorkspace());
        auto result = new NDArray(newShape, true, _context);
        // create temporary dynamic array of extra parameters if array extraParams is empty (==nullptr)
        void* params = extraParams != nullptr ? const_cast<ExtraArguments*>(extraParams)->argumentsAsT(this->dataType()) : nullptr;
        if(params == nullptr) {
            params = new int8_t[result->sizeOfT()*3];
            memset(params, 0, result->sizeOfT()*3);
        }

        auto packX = ConstantTadHelper::getInstance()->tadForDimensions(_shapeInfo, copy);
        auto packY = ConstantTadHelper::getInstance()->tadForDimensions(other->_shapeInfo, copy);

        // perform calculations
        if(rankOf() == copy.size() && other->rankOf() == copy.size())
            NativeOpExecutioner::execReduce3Scalar(_context, op, _buffer, _shapeInfo, _bufferD, _shapeInfoD, params, other->_buffer, other->_shapeInfo, other->_bufferD, other->_shapeInfoD, result->_buffer, result->shapeInfo(), result->specialBuffer(), result->specialShapeInfo());
        else {
            NativeOpExecutioner::execReduce3(_context, op, _buffer, _shapeInfo, _bufferD, _shapeInfoD, params, other->_buffer, other->_shapeInfo, other->_bufferD, other->_shapeInfoD, result->_buffer, result->_shapeInfo, result->_bufferD, result->_shapeInfoD, copy.data(), copy.size(), packX.primaryShapeInfo(), packX.primaryOffsets(), packY.primaryShapeInfo(), packY.primaryOffsets());
        }

        if(params != extraParams)
            delete [] static_cast<int8_t*>(params);

        return result;
    }

    void* NDArray::specialBufferWithOffset(Nd4jLong offset) const {
        return nullptr;
    }

//////////////////////////////////////////////////////////////////////////
    bool NDArray::permutei(const int* dimensions, const int rank) {

        auto shapeInfo = ShapeUtils::evalPermShapeInfo(dimensions, rank, *this, _context->getWorkspace());
        setShapeInfo(shapeInfo);

        return true;
    }

    //////////////////////////////////////////////////////////////////////////
    bool NDArray::permutei(const Nd4jLong* dimensions, const int rank) {

        auto shapeInfo = ShapeUtils::evalPermShapeInfo(dimensions, rank, *this, _context->getWorkspace());
        setShapeInfo(shapeInfo);

        return true;
    }

//////////////////////////////////////////////////////////////////////////
// method reduces array by excluding its shapes along axes present in dimensions vector
void NDArray::reduceAlongDimension(nd4j::reduce::FloatOps op, NDArray* target, const std::vector<int>& dimensions, const bool keepDims, const bool supportOldShapes, const bool checkTargetShape) const {

    if (isS())
        throw std::runtime_error("NDArray::reduceAlongDimension FloatOps: you can't use this method on String array!");
    if (target == nullptr || !target->isR())
        throw std::invalid_argument("NDArray::reduceAlongDimension FloatOps: requires target array to be present and have type form real space!");

    std::vector<int> copy(dimensions);

    if(checkTargetShape) {
        auto newShape = ShapeUtils::evalReduceShapeInfo(target->ordering(), copy, *this, keepDims, supportOldShapes, _context->getWorkspace());
        if(!shape::shapeEquals(newShape, target->getShapeInfo()))
            throw std::runtime_error("NDArray::reduceAlongDimension FloatOps: wrong target shape!");
    }

    if(rankOf() == copy.size() || copy.empty())
        //target->_buffer[0] = functions::reduce::ReduceFloatFunction<T>::template execScalar<OpName>(_buffer, _shapeInfo, extras);
        NativeOpExecutioner::execReduceFloatScalar(_context, op, this->getBuffer(), this->getShapeInfo(), this->getSpecialBuffer(), this->getSpecialShapeInfo(), nullptr, target->buffer(), target->shapeInfo(), target->specialBuffer(), target->specialShapeInfo());
    else {
        auto packX = ConstantTadHelper::getInstance()->tadForDimensions(_shapeInfo, copy);

        NativeOpExecutioner::execReduceFloat(_context, op, this->getBuffer(), this->getShapeInfo(), this->getSpecialBuffer(), this->getSpecialShapeInfo(), nullptr, target->getBuffer(), target->getShapeInfo(), target->getSpecialBuffer(), target->getSpecialShapeInfo(), copy.data(), copy.size(), packX.primaryShapeInfo(), packX.primaryOffsets());
    }
}

//////////////////////////////////////////////////////////////////////////
void NDArray::reduceAlongDimension(nd4j::reduce::SameOps op, NDArray* target, const std::vector<int>& dimensions, const bool keepDims, const bool supportOldShapes, const bool checkTargetShape) const {

    if (isS())
        throw std::runtime_error("NDArray::reduceAlongDimension SameOps: you can't use this method on String array!");
    if (target == nullptr || target->_dataType != _dataType)
        throw std::runtime_error("NDArray::reduceAlongDimension SameOps: requires target array to be present and have same dtype as input");

    std::vector<int> copy(dimensions);

    if(checkTargetShape) {
        auto newShape = ShapeUtils::evalReduceShapeInfo(target->ordering(), copy, *this, keepDims, supportOldShapes, _context->getWorkspace());
        if(!shape::shapeEquals(newShape, target->getShapeInfo()))
            throw std::runtime_error("NDArray::reduceAlongDimension SameOps: wrong target shape!");
    }

    if(rankOf() == copy.size() || copy.empty())
        NativeOpExecutioner::execReduceSameScalar(_context, op, this->getBuffer(), this->getShapeInfo(), this->getSpecialBuffer(), this->getSpecialShapeInfo(), nullptr, target->buffer(), target->shapeInfo(), target->specialBuffer(), target->specialShapeInfo());
    else {
        auto packX = ConstantTadHelper::getInstance()->tadForDimensions(_shapeInfo, copy);

        NativeOpExecutioner::execReduceSame(_context, op, this->getBuffer(), this->getShapeInfo(), this->getSpecialBuffer(), this->getSpecialShapeInfo(), nullptr, target->getBuffer(), target->getShapeInfo(), target->getSpecialBuffer(), target->getSpecialShapeInfo(), copy.data(), copy.size(), packX.primaryShapeInfo(), packX.primaryOffsets());
    }
}

//////////////////////////////////////////////////////////////////////////
void NDArray::reduceAlongDimension(nd4j::reduce::BoolOps op, NDArray* target, const std::vector<int>& dimensions, const bool keepDims, const bool supportOldShapes, const bool checkTargetShape) const {

    if (isS())
        throw std::runtime_error("NDArray::reduceAlongDimension BoolOps: you can't use this method on String array!");
    if (target == nullptr || !target->isB())
        throw std::invalid_argument("NDArray::reduceAlongDimension BoolOps: requires target array to be present and have BOOL type!");

    std::vector<int> copy(dimensions);

    if(checkTargetShape) {
        auto newShape = ShapeUtils::evalReduceShapeInfo(target->ordering(), copy, *this, keepDims, supportOldShapes, _context->getWorkspace());
        if(!shape::shapeEquals(newShape, target->getShapeInfo()))
            throw std::runtime_error("NDArray::reduceAlongDimension BoolOps: wrong target shape!");
    }

    if(rankOf() == copy.size() || copy.empty())
        //target->_buffer[0] = functions::reduce::ReduceFloatFunction<T>::template execScalar<OpName>(_buffer, _shapeInfo, extras);
        NativeOpExecutioner::execReduceBoolScalar(_context, op, this->getBuffer(), this->getShapeInfo(), this->getSpecialBuffer(), this->getSpecialShapeInfo(), nullptr, target->buffer(), target->shapeInfo(), target->specialBuffer(), target->specialShapeInfo());
    else {
        auto packX = ConstantTadHelper::getInstance()->tadForDimensions(_shapeInfo, dimensions);

        NativeOpExecutioner::execReduceBool(_context, op, this->getBuffer(), this->getShapeInfo(), this->getSpecialBuffer(), this->getSpecialShapeInfo(), nullptr, target->getBuffer(), target->getShapeInfo(), target->getSpecialBuffer(), target->getSpecialShapeInfo(), copy.data(), copy.size(), packX.primaryShapeInfo(), packX.primaryOffsets());
    }
}

//////////////////////////////////////////////////////////////////////////
void NDArray::reduceAlongDimension(nd4j::reduce::LongOps op, NDArray* target, const std::vector<int>& dimensions, const bool keepDims, const bool supportOldShapes, const bool checkTargetShape) const {

    if (isS())
        throw std::runtime_error("NDArray::reduceAlongDimension LongOps: you can't use this method on String array!");
    if (target == nullptr || target->_dataType != DataType::INT64)
        throw std::runtime_error("NDArray::reduceAlongDimension LongOps: requires target array to be present and have type of INT64");

    std::vector<int> copy(dimensions);

    if(checkTargetShape) {
        auto newShape = ShapeUtils::evalReduceShapeInfo(target->ordering(), copy, *this, keepDims, supportOldShapes, _context->getWorkspace());
        if(!shape::shapeEquals(newShape, target->getShapeInfo()))
            throw std::runtime_error("NDArray::reduceAlongDimension LongOps: wrong target shape!");
    }

    if(rankOf() == copy.size() || copy.empty())
        NativeOpExecutioner::execReduceLongScalar(_context, op, this->getBuffer(), this->getShapeInfo(), this->getSpecialBuffer(), this->getSpecialShapeInfo(), nullptr, target->buffer(), target->shapeInfo(), target->specialBuffer(), target->specialShapeInfo());
    else {
        auto packX = ConstantTadHelper::getInstance()->tadForDimensions(_shapeInfo, dimensions);

        NativeOpExecutioner::execReduceLong(_context, op, this->getBuffer(), this->getShapeInfo(), this->getSpecialBuffer(), this->getSpecialShapeInfo(), nullptr, target->getBuffer(), target->getShapeInfo(), target->getSpecialBuffer(), target->getSpecialShapeInfo(), copy.data(), copy.size(), packX.primaryShapeInfo(), packX.primaryOffsets());
    }
}

//////////////////////////////////////////////////////////////////////////
// This method sets value in linear buffer to position i
    template <typename T>
    void NDArray::p(const Nd4jLong i, const T value) {

        if (i >= _length)
            throw std::invalid_argument("NDArray::p(i, value): input index is out of array length !");

        auto rp = getOffset(i);
        const void *pV = reinterpret_cast<const void*>(const_cast<T *>(&value));
        BUILD_SINGLE_PARTIAL_SELECTOR(this->dataType(), templatedSet<, T>(this->_buffer, rp, pV), LIBND4J_TYPES);
    }
    template void NDArray::p(const Nd4jLong i, const double value);
    template void NDArray::p(const Nd4jLong i, const float value);
    template void NDArray::p(const Nd4jLong i, const float16 value);
    template void NDArray::p(const Nd4jLong i, const bfloat16 value);
    template void NDArray::p(const Nd4jLong i, const Nd4jLong value);
    template void NDArray::p(const Nd4jLong i, const int value);
    template void NDArray::p(const Nd4jLong i, const int8_t value);
    template void NDArray::p(const Nd4jLong i, const uint8_t value);
    template void NDArray::p(const Nd4jLong i, const int16_t value);
    template void NDArray::p(const Nd4jLong i, const bool value);

    void NDArray::p(const Nd4jLong i, const NDArray& scalar) {
        if(!scalar.isScalar())
            throw std::invalid_argument("NDArray::p method: input array must be scalar!");
        if (i >= _length)
            throw std::invalid_argument("NDArray::p(i, NDArray_scalar): input index is out of array length !");
        // probably wrong args order
        auto rp = getOffset(i);
        BUILD_SINGLE_SELECTOR(scalar.dataType(), templatedSet, (_buffer, rp, scalar.dataType(), scalar.getBuffer()), LIBND4J_TYPES);
        // void NDArray::templatedSet(void *buffer, const Nd4jLong xOfsset, nd4j::DataType dtype, void *value)
    }

//////////////////////////////////////////////////////////////////////////
// This method sets value in 2D matrix to position i, j

    template <typename T>
    void NDArray::p(const Nd4jLong i, const Nd4jLong j, const T value) {
        //(*this)(i,j) = value;
        if (rankOf() != 2 || i >= shapeOf()[0] || j >= shapeOf()[1])
            throw std::invalid_argument("NDArray:pe(i,j, value): one of input indexes is out of array length or rank!=2 !");

        void *p = reinterpret_cast<void *>(const_cast<T *>(&value));
        auto xType = this->dataType();
        Nd4jLong coords[2] = {i, j};
        auto xOffset = shape::getOffset(0, shapeOf(), stridesOf(), coords, rankOf());
        BUILD_SINGLE_PARTIAL_SELECTOR(xType, templatedSet<, T>(this->_buffer, xOffset, p), LIBND4J_TYPES);
    }
    template void NDArray::p(const Nd4jLong i, const Nd4jLong j, const double value);
    template void NDArray::p(const Nd4jLong i, const Nd4jLong j, const float value);
    template void NDArray::p(const Nd4jLong i, const Nd4jLong j, const float16 value);
    template void NDArray::p(const Nd4jLong i, const Nd4jLong j, const bfloat16 value);
    template void NDArray::p(const Nd4jLong i, const Nd4jLong j, const Nd4jLong value);
    template void NDArray::p(const Nd4jLong i, const Nd4jLong j, const int value);
    template void NDArray::p(const Nd4jLong i, const Nd4jLong j, const int8_t value);
    template void NDArray::p(const Nd4jLong i, const Nd4jLong j, const uint8_t value);
    template void NDArray::p(const Nd4jLong i, const Nd4jLong j, const int16_t value);
    template void NDArray::p(const Nd4jLong i, const Nd4jLong j, const bool value);
    // template void NDArray::p(const Nd4jLong i, const Nd4jLong j, const utf8string value);

//////////////////////////////////////////////////////////////////////////
// This method sets value in 3D matrix to position i,j,k
    template <typename T>
    void NDArray::p(const Nd4jLong i, const Nd4jLong j, const Nd4jLong k, const T value) {
        //(*this)(i,j,k) = value;
        if (rankOf() != 3 || i >= shapeOf()[0] || j >= shapeOf()[1] || k >= shapeOf()[2])
            throw std::invalid_argument("NDArray:pe(i,j,k, value): one of input indexes is out of array length or rank!=3 !");
        void *p = reinterpret_cast<void *>(const_cast<T *>(&value));
        auto xType = this->dataType();
        Nd4jLong coords[3] = {i, j, k};
        auto xOffset = shape::getOffset(0, shapeOf(), stridesOf(), coords, rankOf());
        BUILD_SINGLE_PARTIAL_SELECTOR(xType, templatedSet<, T>(this->_buffer, xOffset, p), LIBND4J_TYPES);
    }
    template void NDArray::p(const Nd4jLong i, const Nd4jLong j, const Nd4jLong k, const double value);
    template void NDArray::p(const Nd4jLong i, const Nd4jLong j, const Nd4jLong k, const float value);
    template void NDArray::p(const Nd4jLong i, const Nd4jLong j, const Nd4jLong k, const float16 value);
    template void NDArray::p(const Nd4jLong i, const Nd4jLong j, const Nd4jLong k, const bfloat16 value);
    template void NDArray::p(const Nd4jLong i, const Nd4jLong j, const Nd4jLong k, const Nd4jLong value);
    template void NDArray::p(const Nd4jLong i, const Nd4jLong j, const Nd4jLong k, const int value);
    template void NDArray::p(const Nd4jLong i, const Nd4jLong j, const Nd4jLong k, const int8_t value);
    template void NDArray::p(const Nd4jLong i, const Nd4jLong j, const Nd4jLong k, const uint8_t value);
    template void NDArray::p(const Nd4jLong i, const Nd4jLong j, const Nd4jLong k, const int16_t value);
    template void NDArray::p(const Nd4jLong i, const Nd4jLong j, const Nd4jLong k, const bool value);

    template <typename T>
    void NDArray::p(const Nd4jLong i, const Nd4jLong j, const Nd4jLong k, const Nd4jLong l, const T value) {
        //(*this)(i,j,k) = value;
        if (rankOf() != 4 || i >= shapeOf()[0] || j >= shapeOf()[1] || k >= shapeOf()[2] || l >= shapeOf()[3])
            throw std::invalid_argument("NDArray::p(i,j,k,l, value): one of input indexes is out of array length or rank!=4 !");
        void *p = reinterpret_cast<void *>(const_cast<T *>(&value));
        auto xType = this->dataType();
        Nd4jLong coords[4] = {i, j, k, l};
        auto xOffset = shape::getOffset(0, shapeOf(), stridesOf(), coords, rankOf());
        BUILD_SINGLE_PARTIAL_SELECTOR(xType, templatedSet<, T>(this->_buffer, xOffset, p), LIBND4J_TYPES);
    }
    template void NDArray::p(const Nd4jLong i, const Nd4jLong j, const Nd4jLong k, const Nd4jLong l, const double value);
    template void NDArray::p(const Nd4jLong i, const Nd4jLong j, const Nd4jLong k, const Nd4jLong l, const float value);
    template void NDArray::p(const Nd4jLong i, const Nd4jLong j, const Nd4jLong k, const Nd4jLong l, const float16 value);
    template void NDArray::p(const Nd4jLong i, const Nd4jLong j, const Nd4jLong k, const Nd4jLong l, const bfloat16 value);
    template void NDArray::p(const Nd4jLong i, const Nd4jLong j, const Nd4jLong k, const Nd4jLong l, const Nd4jLong value);
    template void NDArray::p(const Nd4jLong i, const Nd4jLong j, const Nd4jLong k, const Nd4jLong l, const int value);
    template void NDArray::p(const Nd4jLong i, const Nd4jLong j, const Nd4jLong k, const Nd4jLong l, const int8_t value);
    template void NDArray::p(const Nd4jLong i, const Nd4jLong j, const Nd4jLong k, const Nd4jLong l, const uint8_t value);
    template void NDArray::p(const Nd4jLong i, const Nd4jLong j, const Nd4jLong k, const Nd4jLong l, const int16_t value);
    template void NDArray::p(const Nd4jLong i, const Nd4jLong j, const Nd4jLong k, const Nd4jLong l, const bool value);

    NDArray* NDArray::tensorAlongDimension(Nd4jLong index, const std::vector<int>& dimensions) const {
        std::vector<int> copy(dimensions);
        shape::checkDimensions(rankOf(), copy);

        auto packX = ConstantTadHelper::getInstance()->tadForDimensions(_shapeInfo, copy);

        auto tadLength = shape::length(packX.primaryShapeInfo());
        auto numTads = packX.numberOfTads();

        if (index >= numTads)
            throw std::runtime_error("Can't get index higher than total number of TADs");

        auto array = new NDArray(bufferWithOffset(packX.primaryOffsets()[index]), packX.primaryShapeInfo(), _context, false);
        array->_isView = true;

        return array;
    }


//////////////////////////////////////////////////////////////////////////
// Returns value from 2D matrix by coordinates/indexes
    template <typename T>
    T NDArray::e(const Nd4jLong i, const Nd4jLong j) const {
        if (rankOf() != 2 || i >= shapeOf()[0] || j >= shapeOf()[1])
            throw std::invalid_argument("NDArray::e(i,j): one of input indexes is out of array length or rank!=2 !");

        auto xType = this->dataType();
        Nd4jLong coords[2] = {i, j};
        auto xOffset = shape::getOffset(0, shapeOf(), stridesOf(), coords, rankOf());
        //return (*this)(i, j);
        BUILD_SINGLE_PARTIAL_SELECTOR(xType, return templatedGet<, T>(this->_buffer, xOffset), LIBND4J_TYPES);
        return static_cast<T>(119);
    }
    BUILD_SINGLE_UNCHAINED_TEMPLATE(template , NDArray::e(const Nd4jLong, const Nd4jLong) const, LIBND4J_TYPES);

//////////////////////////////////////////////////////////////////////////
// returns value from 3D tensor by coordinates
    template <typename T>
    T NDArray::e(const Nd4jLong i, const Nd4jLong j, const Nd4jLong k) const {
        //return (*this)(i, j, k);
        if (rankOf() != 3 || i >= shapeOf()[0] || j >= shapeOf()[1] || k >= shapeOf()[2])
            throw std::invalid_argument("NDArray::e(i,j,k): one of input indexes is out of array length or rank!=3 !");

        auto xType = this->dataType();
        Nd4jLong coords[3] = {i, j, k};
        auto xOffset = shape::getOffset(0, shapeOf(), stridesOf(), coords, rankOf());
        BUILD_SINGLE_PARTIAL_SELECTOR(xType, return templatedGet<, T>(this->_buffer, xOffset), LIBND4J_TYPES);
        return static_cast<T>(119);
    }
    BUILD_SINGLE_UNCHAINED_TEMPLATE(template , NDArray::e(const Nd4jLong, const Nd4jLong, const Nd4jLong) const, LIBND4J_TYPES);

//////////////////////////////////////////////////////////////////////////
    // returns value from 3D tensor by coordinates
    template <typename T>
    T NDArray::e(const Nd4jLong i, const Nd4jLong j, const Nd4jLong k, const Nd4jLong l) const {
        //return (*this)(i, j, k);
        if (rankOf() != 4 || i >= shapeOf()[0] || j >= shapeOf()[1] || k >= shapeOf()[2] || l >= shapeOf()[3])
            throw std::invalid_argument("NDArray::e(i,j,k,l): one of input indexes is out of array length or rank!=4 !");

        auto xType = this->dataType();
        Nd4jLong coords[4] = {i, j, k, l};
        auto xOffset = shape::getOffset(0, shapeOf(), stridesOf(), coords, rankOf());
        BUILD_SINGLE_PARTIAL_SELECTOR(xType, return templatedGet<, T>(this->_buffer, xOffset), LIBND4J_TYPES);

        return static_cast<T>(119);
    }
    BUILD_SINGLE_UNCHAINED_TEMPLATE(template , NDArray::e(const Nd4jLong, const Nd4jLong, const Nd4jLong, const Nd4jLong) const, LIBND4J_TYPES);

//////////////////////////////////////////////////////////////////////////
NDArray NDArray::e(const Nd4jLong i) const {
    if (i >= _length)
            throw std::invalid_argument("scalar NDArray::e(i): input index is out of array length !");
    NDArray scalar(_dataType, _context);
    BUILD_SINGLE_SELECTOR(_dataType, scalar.templatedSet, (scalar._buffer, 0, dataType(), bufferWithOffset(getOffset(i))), LIBND4J_TYPES);
    return scalar;
}

//////////////////////////////////////////////////////////////////////////
    void NDArray::addRowVector(const NDArray *row, NDArray *target) const {
        if (isS())
            throw std::runtime_error("NDArray::addRowVector: you can't use this method on String array!");
        if (rankOf() != 2 || target->rankOf() != 2 || rows() != target->rows() || columns() != target->columns() || !row->isRowVector() || columns() != row->lengthOf())
            throw std::invalid_argument("NDArray::addRowVector: wrong arguments !");
        if(target->_dataType !=  DataTypeUtils::pickPairwiseResultType(_dataType, row->_dataType) && !(isR() && row->isR() && target->isR()))
            throw std::invalid_argument("NDArray::addRowVector: wrong type of target array !");

        int dimension = 1;
        auto packX = ConstantTadHelper::getInstance()->tadForDimensions(_shapeInfo, &dimension, 1);

        NativeOpExecutioner::execBroadcast(_context, nd4j::broadcast::Ops::Add, _buffer, _shapeInfo, _bufferD, _shapeInfoD, row->_buffer, row->_shapeInfo, row->_bufferD, row->_shapeInfoD, target->getBuffer(), target->getShapeInfo(), target->getSpecialBuffer(), target->getSpecialShapeInfo(), &dimension, 1, packX.primaryShapeInfo(), packX.primaryOffsets(), nullptr, nullptr);
    }

//////////////////////////////////////////////////////////////////////////
    void NDArray::subRowVector(const NDArray *row, NDArray * target) const {

        if (isS())
            throw std::runtime_error("NDArray::subRowVector: you can't use this method on String array!");
        if (rankOf() != 2 || target->rankOf() != 2 || rows() != target->rows() || columns() != target->columns() || !row->isRowVector() || columns() != row->columns())
            throw std::invalid_argument("NDArray::subRowVector: wrong arguments !");
        if(target->_dataType !=  DataTypeUtils::pickPairwiseResultType(_dataType, row->_dataType))
            throw std::invalid_argument("NDArray::subRowVector: wrong type of target array !");

        int dimension = 1;
        auto packX = ConstantTadHelper::getInstance()->tadForDimensions(_shapeInfo, dimension);

        NativeOpExecutioner::execBroadcast(_context, nd4j::broadcast::Ops::Subtract, _buffer, _shapeInfo, _bufferD, _shapeInfoD, row->_buffer, row->_shapeInfo, row->_bufferD, row->_shapeInfoD, target->getBuffer(), target->getShapeInfo(), target->getSpecialBuffer(), target->getSpecialShapeInfo(), &dimension, 1, packX.primaryShapeInfo(), packX.primaryOffsets(), nullptr, nullptr);
    }

//////////////////////////////////////////////////////////////////////////
    void NDArray::mulRowVector(const NDArray *row, NDArray *target) const {

        if (isS())
            throw std::runtime_error("NDArray::mulRowVector: you can't use this method on String array!");
        if (rankOf() != 2 || target->rankOf() != 2 || rows() != target->rows() || columns() != target->columns() || !row->isRowVector() || columns() != row->columns())
            throw std::invalid_argument("NDArray::divRowVector: wrong arguments !");
        if(target->_dataType !=  DataTypeUtils::pickPairwiseResultType(_dataType, row->_dataType))
            throw std::invalid_argument("NDArray::mulRowVector: wrong type of target array !");

        int dimension = 1;
        auto packX = ConstantTadHelper::getInstance()->tadForDimensions(_shapeInfo, dimension);

        NativeOpExecutioner::execBroadcast(_context, nd4j::broadcast::Ops::Multiply, _buffer, _shapeInfo, _bufferD, _shapeInfoD, row->_buffer, row->_shapeInfo, row->_bufferD, row->_shapeInfoD, target->getBuffer(), target->getShapeInfo(), target->getSpecialBuffer(), target->getSpecialShapeInfo(), &dimension, 1, packX.primaryShapeInfo(), packX.primaryOffsets(), nullptr, nullptr);
    }

//////////////////////////////////////////////////////////////////////////
    void NDArray::divRowVector(const NDArray *row, NDArray *target) const {

        if (isS())
            throw std::runtime_error("NDArray::divRowVector: you can't use this method on String array!");
        if (row->isB())
            throw std::runtime_error("NDArray::divRowVector: you can't divide by bool row!");
        if (rankOf() != 2 || target->rankOf() != 2 || rows() != target->rows() || columns() != target->columns() || !row->isRowVector() || columns() != row->columns())
            throw std::invalid_argument("NDArray::divRowVector: wrong arguments !");
        if(target->_dataType !=  DataTypeUtils::pickPairwiseResultType(_dataType, row->_dataType))
            throw std::invalid_argument("NDArray::divRowVector: wrong type of target array !");

        int dimension = 1;
        auto packX = ConstantTadHelper::getInstance()->tadForDimensions(_shapeInfo, dimension);

        NativeOpExecutioner::execBroadcast(_context, nd4j::broadcast::Divide, _buffer, _shapeInfo, _bufferD, _shapeInfoD, row->_buffer, row->_shapeInfo, row->_bufferD, row->_shapeInfoD, target->getBuffer(), target->getShapeInfo(), target->getSpecialBuffer(), target->getSpecialShapeInfo(), &dimension, 1, packX.primaryShapeInfo(), packX.primaryOffsets(), nullptr, nullptr);
    }

//////////////////////////////////////////////////////////////////////////
// This method adds given row to all rows in this NDArray, this array becomes affected
    void NDArray::addiRowVector(const NDArray *row) {

        if (isS())
            throw std::runtime_error("NDArray::addiRowVector: you can't use this method on String array!");
        if (rankOf() != 2 || !row->isRowVector() || columns() != row->lengthOf())
            throw std::invalid_argument("NDArray::addiRowVector: wrong arguments !");

        int dimension = 1;
        auto packX = ConstantTadHelper::getInstance()->tadForDimensions(_shapeInfo, dimension);

        NativeOpExecutioner::execBroadcast(_context, nd4j::broadcast::Ops::Add, _buffer, _shapeInfo, _bufferD, _shapeInfoD, row->_buffer, row->_shapeInfo, row->_bufferD, row->_shapeInfoD, this->buffer(), this->shapeInfo(), this->specialBuffer(), this->specialShapeInfo(), &dimension, 1, packX.primaryShapeInfo(), packX.primaryOffsets(), nullptr, nullptr);
    }

//////////////////////////////////////////////////////////////////////////
    void NDArray::addColumnVector(const NDArray *column, NDArray *target) const {
        if (isS())
            throw std::runtime_error("NDArray::addColumnVector: you can't use this method on String array!");
        if (rankOf() != 2 || target->rankOf() != 2 || rows() != target->rows() || columns() != target->columns() || !column->isColumnVector() || rows() != column->lengthOf())
            throw std::invalid_argument("NDArray::addColumnVector: wrong arguments !");
        if(target->_dataType !=  DataTypeUtils::pickPairwiseResultType(_dataType, column->_dataType))
            throw std::invalid_argument("NDArray::addColumnVector: wrong type of target array !");

        int dimension = 0;
        auto packX = ConstantTadHelper::getInstance()->tadForDimensions(_shapeInfo, dimension);

        NativeOpExecutioner::execBroadcast(_context, nd4j::broadcast::Ops::Add, _buffer, _shapeInfo, _bufferD, _shapeInfoD, column->_buffer, column->_shapeInfo, column->_bufferD, column->_shapeInfoD, target->getBuffer(), target->getShapeInfo(), target->getSpecialBuffer(), target->getSpecialShapeInfo(), &dimension, 1, packX.primaryShapeInfo(), packX.primaryOffsets(), nullptr, nullptr);
    }

//////////////////////////////////////////////////////////////////////////
// This method adds given column to all columns in this NDArray, this array becomes affected
    void NDArray::addiColumnVector(const NDArray *column) {
        if (isS())
            throw std::runtime_error("NDArray::addiColumnVector: you can't use this method on String array!");
        if (rankOf() != 2 || !column->isColumnVector() || rows() != column->lengthOf())
            throw std::invalid_argument("NDArray::addiColumnVector: wrong arguments !");

        int dimension = 0;
        auto packX = ConstantTadHelper::getInstance()->tadForDimensions(_shapeInfo, dimension);

        NativeOpExecutioner::execBroadcast(_context, nd4j::broadcast::Ops::Add, _buffer, _shapeInfo, _bufferD, _shapeInfoD, column->_buffer, column->_shapeInfo, column->_bufferD, column->_shapeInfoD, this->buffer(), this->shapeInfo(), this->specialBuffer(), this->specialShapeInfo(), &dimension, 1, packX.primaryShapeInfo(), packX.primaryOffsets(), nullptr, nullptr);
    }

//////////////////////////////////////////////////////////////////////////
// This method multiplies each column of this array by given argument-column, this array becomes affected
    void NDArray::muliColumnVector(const NDArray *column) {
        if (isS())
            throw std::runtime_error("NDArray::muliColumnVector: you can't use this method on String array!");
        if (rankOf() != 2 || !column->isColumnVector() || rows() != column->lengthOf())
            throw std::invalid_argument("NDArray::muliColumnVector: wrong arguments !");

        int dimension = 0;
        auto packX = ConstantTadHelper::getInstance()->tadForDimensions(_shapeInfo, dimension);

        NativeOpExecutioner::execBroadcast(_context, nd4j::broadcast::Ops::Multiply, _buffer, _shapeInfo, _bufferD, _shapeInfoD, column->_buffer, column->_shapeInfo, column->_bufferD, column->_shapeInfoD, this->buffer(), this->shapeInfo(), this->specialBuffer(), this->specialShapeInfo(), &dimension, 1, packX.primaryShapeInfo(), packX.primaryOffsets(), nullptr, nullptr);
    }

    //////////////////////////////////////////////////////////////////////////
    // change an array by repeating it the number of times given by reps.
    NDArray NDArray::tile(const std::vector<Nd4jLong>& reps) const {
        int dim = reps.size();
        int product = 1;
        for(const auto& item : reps)
            product *= item;
        if(product == 0)
            throw std::runtime_error("NDArray::tile method: one of the elements in reps array is zero !");

        int rankOld = rankOf();
        int diff = rankOld - dim;
        if(product==1) {        // in this case 2 possibilities are present: just reshape or nothing to do
            NDArray result(*this);
            if(diff < 0) {      // reshape to higher dimension
                std::vector<Nd4jLong> shapeNew = reps;               // need to have unities at first "diff" positions of new shape
                memcpy(&shapeNew[-diff], result._shapeInfo+1, rankOld * sizeof(Nd4jLong));   // put old shape numbers at rest of positions
                result.reshapei(ordering(), shapeNew);
            }
            return result;             // nothing to do, if diff >= 0 -> identity tile
        }

        // evaluate shapeInfo for resulting array
        auto newShapeInfo = ShapeUtils::evalTileShapeInfo(*this, reps, _context->getWorkspace());
        // create new buffer, in any case the memory amount new buffer points to is bigger then those for old _buffer
        int8_t * newBuff = nullptr;
        ALLOCATE(newBuff, _context->getWorkspace(), shape::length(newShapeInfo) * sizeOfT(), int8_t);
        // assign new shape and new buffer to resulting array
        NDArray result(newBuff, newShapeInfo, _context, true);

        // fill newBuff, loop through all elements of newBuff
        // looping through _buffer goes automatically by means of getSubArrayIndex applying
        const auto resultLen = result.lengthOf();
        auto xType = this->dataType();
        if(result.ordering() == 'c') {           //  ews == 1 always here

            PRAGMA_OMP_PARALLEL_FOR_SIMD
            for(Nd4jLong i=0;  i<resultLen; ++i) {
                auto yOffset = shape::subArrayOffset(i, newShapeInfo, _shapeInfo);
                BUILD_SINGLE_SELECTOR(xType, this->template templatedAssign, (newBuff, i, this->_buffer, yOffset), LIBND4J_TYPES);

            }
        }
        else {

            PRAGMA_OMP_PARALLEL_FOR_SIMD
            for(int i=0;  i<resultLen; ++i) {
                auto xOffset = result.getOffset(i);
                auto yOffset = shape::subArrayOffset(i, newShapeInfo, _shapeInfo);
                BUILD_SINGLE_SELECTOR(xType, this->template templatedAssign, (newBuff, xOffset, this->_buffer, yOffset), LIBND4J_TYPES);
            }
        }
        result.tickWriteHost();
        return result;
    }

    template <typename T>
    void NDArray::templatedAssign(void *xBuffer, Nd4jLong xOffset, const void *yBuffer, const Nd4jLong yOffset) const {
        auto x = reinterpret_cast<T *>(xBuffer);
        const auto y = reinterpret_cast<const T*>(yBuffer);
        if (xBuffer != nullptr && yBuffer != nullptr)
            x[xOffset] = y[yOffset];
    }
    BUILD_SINGLE_TEMPLATE(template void NDArray::templatedAssign, (void *xBuffer, const Nd4jLong xOffset, const void *yBuffer, const Nd4jLong yOffset) const, LIBND4J_TYPES);

    //////////////////////////////////////////////////////////////////////////
    // change an array by repeating it the number of times given by reps.
    void NDArray::tile(const std::vector<Nd4jLong>& reps, NDArray& target) const {

        // evaluate true tile shapeInfo for comparison with target shapeInfo
        auto newShapeInfo = ShapeUtils::evalTileShapeInfo(*this, reps, _context->getWorkspace());
        if(!shape::equalsSoft(newShapeInfo, target.getShapeInfo()))  {
            delete []newShapeInfo;
            throw std::runtime_error("NDArray::tile method - shapeInfo of target array is not suitable for tile operation !");
        }

        // fill newBuff, loop through all elements of newBuff
        // looping through _buffer goes automatically by means of getSubArrayIndex applying
        const int ews = target.ews();
        const int targetLen = target.lengthOf();
        if(target.ordering() == 'c' && ews == 1) {           //  ews == 1 always here
//#pragma omp parallel for simd if(targetLen > Environment::getInstance()->elementwiseThreshold()) schedule(guided)
            for(Nd4jLong i=0;  i<targetLen; ++i) {
                auto yOffset = shape::subArrayOffset(i, target._shapeInfo, _shapeInfo);
                BUILD_DOUBLE_SELECTOR(target._dataType, _dataType, templatedDoubleAssign, (target._buffer, i, _buffer, yOffset), LIBND4J_TYPES, LIBND4J_TYPES);
            }
        }
        else if(target.ordering() == 'c' && ews > 1) {
//#pragma omp parallel for simd if(targetLen > Environment::getInstance()->elementwiseThreshold()) schedule(guided)
            for(int i=0;  i<targetLen; ++i) {
                auto yOffset = shape::subArrayOffset(i, target._shapeInfo, _shapeInfo);
                BUILD_DOUBLE_SELECTOR(target._dataType, _dataType, templatedDoubleAssign, (target._buffer, i*ews, _buffer, yOffset), LIBND4J_TYPES, LIBND4J_TYPES);
            }
        }
        else {
//#pragma omp parallel for simd if(targetLen > Environment::getInstance()->elementwiseThreshold()) schedule(guided)
            for(int i=0;  i<targetLen; ++i) {

                auto xOffset = target.getOffset(i);
                auto yOffset = shape::subArrayOffset(i, target._shapeInfo, _shapeInfo);
                BUILD_DOUBLE_SELECTOR(target._dataType, _dataType, templatedDoubleAssign, (target._buffer, xOffset, _buffer, yOffset), LIBND4J_TYPES, LIBND4J_TYPES);
            }
        }
    }

    //////////////////////////////////////////////////////////////////////////
    void NDArray::tile(NDArray& target) const {
        if(rankOf() > target.rankOf())
            throw std::runtime_error("NDArray::tile method - rank of target array must be bigger or equal to the rank of this array !");

        if(!ShapeUtils::areShapesBroadcastable(*this, target))
            throw std::runtime_error("NDArray::tile method - shapeInfo of target array is not suitable for tile operation !");

        // fill newBuff, loop through all elements of newBuff
        // looping through _buffer goes automatically by means of getSubArrayIndex applying
        const auto ews = target.ews();
        const auto targetLen = target.lengthOf();
        if(target.ordering() == 'c' && ews == 1) {           //  ews == 1 always here
//#pragma omp parallel for simd if(targetLen > Environment::getInstance()->elementwiseThreshold()) schedule(guided)
            for (int i = 0; i < targetLen; ++i) {
                auto yOffset = shape::subArrayOffset(i, target._shapeInfo, _shapeInfo);
                BUILD_DOUBLE_SELECTOR(target._dataType, _dataType, templatedDoubleAssign, (target._buffer, i, _buffer, yOffset), LIBND4J_TYPES, LIBND4J_TYPES);
            }
        }
        else if(target.ordering() == 'c' && ews > 1) {
//#pragma omp parallel for simd if(targetLen > Environment::getInstance()->elementwiseThreshold()) schedule(guided)
            for(int i=0;  i<targetLen; ++i) {
                auto yOffset = shape::subArrayOffset(i, target._shapeInfo, _shapeInfo);
                BUILD_DOUBLE_SELECTOR(target._dataType, _dataType, templatedDoubleAssign, (target._buffer, i*ews, _buffer, yOffset), LIBND4J_TYPES, LIBND4J_TYPES);
            }
        }
        else {

//#pragma omp parallel for simd if(targetLen > Environment::getInstance()->elementwiseThreshold()) schedule(guided)
            for(int i=0;  i<targetLen; ++i) {

                auto xOffset = target.getOffset(i);
                auto yOffset = shape::subArrayOffset(i, target._shapeInfo, _shapeInfo);
                BUILD_DOUBLE_SELECTOR(target._dataType, _dataType, templatedDoubleAssign, (target._buffer, xOffset, _buffer, yOffset), LIBND4J_TYPES, LIBND4J_TYPES);
            }
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // create new  array by repeating it the number of times given by reps
    NDArray* NDArray::repeat(int dimension, const std::vector<Nd4jLong>& repeats) const {
        auto outShape = ShapeUtils::evalRepeatShape(dimension, repeats, *this);

        // the size of outShape == rank
        int rank = rankOf();            // = outShape.size()

        std::vector<Nd4jLong> newShape(rank);
        for (int i = 0; i < rank; i++)
            newShape[i] = outShape[i];

        auto ret = new NDArray('c', outShape, _dataType,  _context);

        auto repeatDelta = shape::prodLong(newShape.data(), rank) / this->lengthOf();
        auto numTads = this->tensorsAlongDimension({dimension});
        printf("Repeat delta %lld, numTads %lld\n", repeatDelta, numTads);
        for (int i = 0; i < numTads; i++) {
            auto thisTensor = this->tensorAlongDimension(i, {dimension});
            auto retTensor = ret->tensorAlongDimension(i, {dimension});
            Nd4jLong retIdx = 0;

            for (Nd4jLong k = 0; k < thisTensor->lengthOf(); k++) {
                auto s = thisTensor->e(k);
                for (Nd4jLong j = 0; j < repeatDelta; j++) {
                    retTensor->p(retIdx++, s);
                    printf("Iteration is %lld\n", retIdx);
                }
            }
//            if (isR()) {
//            } else {
//                for (int k = 0; k < thisTensor->lengthOf(); k++) {
//                    auto s = thisTensor->e<Nd4jLong>(k);
//                    for (int j = 0; j < repeatDelta; j++) {
//                        retTensor->p<Nd4jLong>(retIdx++, s);
//                    }
//                }
//            }

            delete thisTensor;
            delete retTensor;
        }

        return ret;
    }

    //////////////////////////////////////////////////////////////////////////
    // fill array by repeating it the number of times given by reps
    void NDArray::repeat(int dimension, NDArray& target) const {

        if(dimension < 0)
            dimension += rankOf();

        if(rankOf() != target.rankOf())
            throw std::invalid_argument("NDArray::repeat(int dimension, NDArray& target) method: wrong rank of target array it must be equal to this array rank!");

        Nd4jLong repeatDelta = target.sizeAt(dimension) / sizeAt(dimension);

        if(repeatDelta == 0)
            throw std::invalid_argument("NDArray::repeat(int dimension, NDArray& target) method: wrong shape of target array!");


        std::vector<int> dimsToExclude = ShapeUtils::evalDimsToExclude(rankOf(), {dimension});
        const Nd4jLong numTads = ShapeUtils::getNumOfSubArrs(_shapeInfo, dimsToExclude);

        for (int i = 0; i < numTads; i++) {
            auto thisTensor = (*this)(i, dimsToExclude);
            auto retTensor = target(i, dimsToExclude);
            int tensorLength = thisTensor.lengthOf();
            int retIdx = 0;
            if (isR()) {
                for (int k = 0; k < tensorLength; k++) {
                    auto s = thisTensor.e<double>(k);
                    for (int j = 0; j < repeatDelta; j++) {
                        retTensor.p<double>(retIdx++, s);
                    }
                }
            } else {
                for (int k = 0; k < tensorLength; k++) {
                    auto s = thisTensor.e<Nd4jLong>(k);
                    for (int j = 0; j < repeatDelta; j++) {
                        retTensor.p<Nd4jLong>(retIdx++, s);
                    }
                }
            }
        }
    }

    //////////////////////////////////////////////////////////////////////////
    template <typename T>
    void NDArray::printCurrentBuffer(const bool host, const char* msg, const int precision) const {

    }


    //BUILD_DOUBLE_TEMPLATE(template void NDArray::templatedSet, (void *buffer, const Nd4jLong *indices, Y value), LIBND4J_TYPES, LIBND4J_TYPES);
/*
#ifndef __CLION_IDE__
#include "NDArray.macro"
#endif
 */
}

#endif

