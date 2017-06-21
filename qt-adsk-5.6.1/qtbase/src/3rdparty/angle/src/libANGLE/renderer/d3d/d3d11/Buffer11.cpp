//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Buffer11.cpp Defines the Buffer11 class.

#include "libANGLE/renderer/d3d/d3d11/Buffer11.h"

#include "common/MemoryBuffer.h"
#include "libANGLE/renderer/d3d/d3d11/Renderer11.h"
#include "libANGLE/renderer/d3d/d3d11/formatutils11.h"

#if defined(ANGLE_MINGW32_COMPAT)
typedef enum D3D11_MAP_FLAG {
    D3D11_MAP_FLAG_DO_NOT_WAIT = 0x100000
} D3D11_MAP_FLAG;
#endif

namespace rx
{

PackPixelsParams::PackPixelsParams()
  : format(GL_NONE),
    type(GL_NONE),
    outputPitch(0),
    packBuffer(NULL),
    offset(0)
{}

PackPixelsParams::PackPixelsParams(const gl::Rectangle &areaIn, GLenum formatIn, GLenum typeIn, GLuint outputPitchIn,
                                   const gl::PixelPackState &packIn, ptrdiff_t offsetIn)
  : area(areaIn),
    format(formatIn),
    type(typeIn),
    outputPitch(outputPitchIn),
    packBuffer(packIn.pixelBuffer.get()),
    pack(packIn.alignment, packIn.reverseRowOrder),
    offset(offsetIn)
{}

namespace gl_d3d11
{

D3D11_MAP GetD3DMapTypeFromBits(GLbitfield access)
{
    bool readBit = ((access & GL_MAP_READ_BIT) != 0);
    bool writeBit = ((access & GL_MAP_WRITE_BIT) != 0);

    ASSERT(readBit || writeBit);

    // Note : we ignore the discard bit, because in D3D11, staging buffers
    //  don't accept the map-discard flag (discard only works for DYNAMIC usage)

    if (readBit && !writeBit)
    {
        return D3D11_MAP_READ;
    }
    else if (writeBit && !readBit)
    {
        return D3D11_MAP_WRITE;
    }
    else if (writeBit && readBit)
    {
        return D3D11_MAP_READ_WRITE;
    }
    else
    {
        UNREACHABLE();
        return D3D11_MAP_READ;
    }
}

}

// Each instance of Buffer11::BufferStorage is specialized for a class of D3D binding points
// - vertex/transform feedback buffers
// - index buffers
// - pixel unpack buffers
// - uniform buffers
class Buffer11::BufferStorage : angle::NonCopyable
{
  public:
    virtual ~BufferStorage() {}

    DataRevision getDataRevision() const { return mRevision; }
    BufferUsage getUsage() const { return mUsage; }
    size_t getSize() const { return mBufferSize; }
    void setDataRevision(DataRevision rev) { mRevision = rev; }

    virtual bool isMappable() const = 0;

    virtual bool copyFromStorage(BufferStorage *source, size_t sourceOffset,
                                 size_t size, size_t destOffset) = 0;
    virtual gl::Error resize(size_t size, bool preserveData) = 0;

    virtual uint8_t *map(size_t offset, size_t length, GLbitfield access) = 0;
    virtual void unmap() = 0;

    gl::Error setData(const uint8_t *data, size_t offset, size_t size);

  protected:
    BufferStorage(Renderer11 *renderer, BufferUsage usage);

    Renderer11 *mRenderer;
    DataRevision mRevision;
    const BufferUsage mUsage;
    size_t mBufferSize;
};

// A native buffer storage represents an underlying D3D11 buffer for a particular
// type of storage.
class Buffer11::NativeStorage : public Buffer11::BufferStorage
{
  public:
    NativeStorage(Renderer11 *renderer, BufferUsage usage);
    ~NativeStorage() override;

    bool isMappable() const override { return mUsage == BUFFER_USAGE_STAGING; }

    ID3D11Buffer *getNativeStorage() const { return mNativeStorage; }

    bool copyFromStorage(BufferStorage *source, size_t sourceOffset,
                         size_t size, size_t destOffset) override;
    gl::Error resize(size_t size, bool preserveData) override;

    uint8_t *map(size_t offset, size_t length, GLbitfield access) override;
    void unmap() override;

  private:
    static void fillBufferDesc(D3D11_BUFFER_DESC* bufferDesc, Renderer11 *renderer, BufferUsage usage, unsigned int bufferSize);

    ID3D11Buffer *mNativeStorage;
};

// Pack storage represents internal storage for pack buffers. We implement pack buffers
// as CPU memory, tied to a staging texture, for asynchronous texture readback.
class Buffer11::PackStorage : public Buffer11::BufferStorage
{
  public:
    explicit PackStorage(Renderer11 *renderer);
    ~PackStorage() override;

    bool isMappable() const override { return true; }

    bool copyFromStorage(BufferStorage *source, size_t sourceOffset,
                         size_t size, size_t destOffset) override;
    gl::Error resize(size_t size, bool preserveData) override;

    uint8_t *map(size_t offset, size_t length, GLbitfield access) override;
    void unmap() override;

    gl::Error packPixels(ID3D11Texture2D *srcTexure, UINT srcSubresource, const PackPixelsParams &params);

  private:
    gl::Error flushQueuedPackCommand();

    ID3D11Texture2D *mStagingTexture;
    DXGI_FORMAT mTextureFormat;
    gl::Extents mTextureSize;
    MemoryBuffer mMemoryBuffer;
    PackPixelsParams *mQueuedPackCommand;
    PackPixelsParams mPackParams;
    bool mDataModified;
};

// System memory storage stores a CPU memory buffer with our buffer data.
// For dynamic data, it's much faster to update the CPU memory buffer than
// it is to update a D3D staging buffer and read it back later.
class Buffer11::SystemMemoryStorage : public Buffer11::BufferStorage
{
  public:
    explicit SystemMemoryStorage(Renderer11 *renderer);
    ~SystemMemoryStorage() override {}

    bool isMappable() const override { return true; }

    bool copyFromStorage(BufferStorage *source, size_t sourceOffset,
                         size_t size, size_t destOffset) override;
    gl::Error resize(size_t size, bool preserveData) override;

    uint8_t *map(size_t offset, size_t length, GLbitfield access) override;
    void unmap() override;

    MemoryBuffer *getSystemCopy() { return &mSystemCopy; }

  protected:
    MemoryBuffer mSystemCopy;
};

Buffer11::Buffer11(Renderer11 *renderer)
    : BufferD3D(renderer),
      mRenderer(renderer),
      mSize(0),
      mMappedStorage(NULL),
      mReadUsageCount(0),
      mHasSystemMemoryStorage(false)
{}

Buffer11::~Buffer11()
{
    for (auto it = mBufferStorages.begin(); it != mBufferStorages.end(); it++)
    {
        SafeDelete(it->second);
    }
}

Buffer11 *Buffer11::makeBuffer11(BufferImpl *buffer)
{
    ASSERT(HAS_DYNAMIC_TYPE(Buffer11*, buffer));
    return static_cast<Buffer11*>(buffer);
}

gl::Error Buffer11::setData(const void *data, size_t size, GLenum usage)
{
    gl::Error error = setSubData(data, size, 0);
    if (error.isError())
    {
        return error;
    }

    if (usage == GL_STATIC_DRAW)
    {
        initializeStaticData();
    }

    return error;
}

gl::Error Buffer11::getData(const uint8_t **outData)
{
    SystemMemoryStorage *systemMemoryStorage = nullptr;
    gl::Error error = getSystemMemoryStorage(&systemMemoryStorage);

    if (error.isError())
    {
        *outData = nullptr;
        return error;
    }

    mReadUsageCount = 0;

    ASSERT(systemMemoryStorage->getSize() >= mSize);

    *outData = systemMemoryStorage->getSystemCopy()->data();
    return gl::Error(GL_NO_ERROR);
}

gl::Error Buffer11::getSystemMemoryStorage(SystemMemoryStorage **storageOut)
{
    BufferStorage *memStorageUntyped = getBufferStorage(BUFFER_USAGE_SYSTEM_MEMORY);

    if (memStorageUntyped == nullptr)
    {
        // TODO(jmadill): convert all to errors
        return gl::Error(GL_OUT_OF_MEMORY);
    }

    *storageOut = GetAs<SystemMemoryStorage>(memStorageUntyped);
    return gl::Error(GL_NO_ERROR);
}

gl::Error Buffer11::setSubData(const void *data, size_t size, size_t offset)
{
    size_t requiredSize = size + offset;

    if (data && size > 0)
    {
        // Use system memory storage for dynamic buffers.

        BufferStorage *writeBuffer = nullptr;
        if (supportsDirectBinding())
        {
            writeBuffer = getStagingStorage();

            if (!writeBuffer)
            {
                return gl::Error(GL_OUT_OF_MEMORY, "Failed to allocate internal buffer.");
            }
        }
        else
        {
            SystemMemoryStorage *systemMemoryStorage = nullptr;
            gl::Error error = getSystemMemoryStorage(&systemMemoryStorage);
            if (error.isError())
            {
                return error;
            }

            writeBuffer = systemMemoryStorage;
        }

        ASSERT(writeBuffer);

        // Explicitly resize the staging buffer, preserving data if the new data will not
        // completely fill the buffer
        if (writeBuffer->getSize() < requiredSize)
        {
            bool preserveData = (offset > 0);
            gl::Error error = writeBuffer->resize(requiredSize, preserveData);
            if (error.isError())
            {
                return error;
            }
        }

        writeBuffer->setData(static_cast<const uint8_t *>(data), offset, size);
        writeBuffer->setDataRevision(writeBuffer->getDataRevision() + 1);
    }

    mSize = std::max(mSize, requiredSize);
    invalidateStaticData();

    return gl::Error(GL_NO_ERROR);
}

gl::Error Buffer11::copySubData(BufferImpl* source, GLintptr sourceOffset, GLintptr destOffset, GLsizeiptr size)
{
    Buffer11 *sourceBuffer = makeBuffer11(source);
    ASSERT(sourceBuffer != NULL);

    BufferStorage *copyDest = getLatestBufferStorage();
    if (!copyDest)
    {
        copyDest = getStagingStorage();
    }

    BufferStorage *copySource = sourceBuffer->getLatestBufferStorage();

    if (!copySource || !copyDest)
    {
        return gl::Error(GL_OUT_OF_MEMORY, "Failed to allocate internal staging buffer.");
    }

    // If copying to/from a pixel pack buffer, we must have a staging or
    // pack buffer partner, because other native buffers can't be mapped
    if (copyDest->getUsage() == BUFFER_USAGE_PIXEL_PACK && !copySource->isMappable())
    {
        copySource = sourceBuffer->getStagingStorage();
    }
    else if (copySource->getUsage() == BUFFER_USAGE_PIXEL_PACK && !copyDest->isMappable())
    {
        copyDest = getStagingStorage();
    }

    // D3D11 does not allow overlapped copies until 11.1, and only if the
    // device supports D3D11_FEATURE_DATA_D3D11_OPTIONS::CopyWithOverlap
    // Get around this via a different source buffer
    if (copySource == copyDest)
    {
        if (copySource->getUsage() == BUFFER_USAGE_STAGING)
        {
            copySource = getBufferStorage(BUFFER_USAGE_VERTEX_OR_TRANSFORM_FEEDBACK);
        }
        else
        {
            copySource = getStagingStorage();
        }
    }

    copyDest->copyFromStorage(copySource, sourceOffset, size, destOffset);
    copyDest->setDataRevision(copyDest->getDataRevision() + 1);

    mSize = std::max<size_t>(mSize, destOffset + size);
    invalidateStaticData();

    return gl::Error(GL_NO_ERROR);
}

gl::Error Buffer11::map(size_t offset, size_t length, GLbitfield access, GLvoid **mapPtr)
{
    ASSERT(!mMappedStorage);

    BufferStorage *latestStorage = getLatestBufferStorage();
    if (latestStorage &&
        (latestStorage->getUsage() == BUFFER_USAGE_PIXEL_PACK ||
         latestStorage->getUsage() == BUFFER_USAGE_STAGING))
    {
        // Latest storage is mappable.
        mMappedStorage = latestStorage;
    }
    else
    {
        // Fall back to using the staging buffer if the latest storage does
        // not exist or is not CPU-accessible.
        mMappedStorage = getStagingStorage();
    }

    if (!mMappedStorage)
    {
        return gl::Error(GL_OUT_OF_MEMORY, "Failed to allocate mappable internal buffer.");
    }

    if ((access & GL_MAP_WRITE_BIT) > 0)
    {
        // Update the data revision immediately, since the data might be changed at any time
        mMappedStorage->setDataRevision(mMappedStorage->getDataRevision() + 1);
    }

    uint8_t *mappedBuffer = mMappedStorage->map(offset, length, access);
    if (!mappedBuffer)
    {
        return gl::Error(GL_OUT_OF_MEMORY, "Failed to map internal buffer.");
    }

    *mapPtr = static_cast<GLvoid *>(mappedBuffer);
    return gl::Error(GL_NO_ERROR);
}

gl::Error Buffer11::unmap()
{
    ASSERT(mMappedStorage);
    mMappedStorage->unmap();
    mMappedStorage = NULL;
    return gl::Error(GL_NO_ERROR);
}

void Buffer11::markTransformFeedbackUsage()
{
    BufferStorage *transformFeedbackStorage = getBufferStorage(BUFFER_USAGE_VERTEX_OR_TRANSFORM_FEEDBACK);

    if (transformFeedbackStorage)
    {
        transformFeedbackStorage->setDataRevision(transformFeedbackStorage->getDataRevision() + 1);
    }

    invalidateStaticData();
}

void Buffer11::markBufferUsage()
{
    mReadUsageCount++;

    // Free the system memory storage if we decide it isn't being used very often.
    const unsigned int usageLimit = 5;

    if (mReadUsageCount > usageLimit && mHasSystemMemoryStorage)
    {
        auto systemMemoryStorageIt = mBufferStorages.find(BUFFER_USAGE_SYSTEM_MEMORY);
        ASSERT(systemMemoryStorageIt != mBufferStorages.end());

        SafeDelete(systemMemoryStorageIt->second);
        mBufferStorages.erase(systemMemoryStorageIt);
        mHasSystemMemoryStorage = false;
    }
}

ID3D11Buffer *Buffer11::getBuffer(BufferUsage usage)
{
    markBufferUsage();

    BufferStorage *bufferStorage = getBufferStorage(usage);

    if (!bufferStorage)
    {
        // Storage out-of-memory
        return NULL;
    }

    ASSERT(HAS_DYNAMIC_TYPE(NativeStorage*, bufferStorage));

    return static_cast<NativeStorage*>(bufferStorage)->getNativeStorage();
}

ID3D11ShaderResourceView *Buffer11::getSRV(DXGI_FORMAT srvFormat)
{
    BufferStorage *storage = getBufferStorage(BUFFER_USAGE_PIXEL_UNPACK);

    if (!storage)
    {
        // Storage out-of-memory
        return NULL;
    }

    ASSERT(HAS_DYNAMIC_TYPE(NativeStorage*, storage));
    ID3D11Buffer *buffer = static_cast<NativeStorage*>(storage)->getNativeStorage();

    auto bufferSRVIt = mBufferResourceViews.find(srvFormat);

    if (bufferSRVIt != mBufferResourceViews.end())
    {
        if (bufferSRVIt->second.first == buffer)
        {
            return bufferSRVIt->second.second;
        }
        else
        {
            // The underlying buffer has changed since the SRV was created: recreate the SRV.
            SafeRelease(bufferSRVIt->second.second);
        }
    }

    ID3D11Device *device = mRenderer->getDevice();
    ID3D11ShaderResourceView *bufferSRV = NULL;

    const d3d11::DXGIFormat &dxgiFormatInfo = d3d11::GetDXGIFormatInfo(srvFormat);

    D3D11_SHADER_RESOURCE_VIEW_DESC bufferSRVDesc;
    bufferSRVDesc.Buffer.ElementOffset = 0;
    bufferSRVDesc.Buffer.ElementWidth = mSize / dxgiFormatInfo.pixelBytes;
    bufferSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    bufferSRVDesc.Format = srvFormat;

    HRESULT result = device->CreateShaderResourceView(buffer, &bufferSRVDesc, &bufferSRV);
    UNUSED_ASSERTION_VARIABLE(result);
    ASSERT(SUCCEEDED(result));

    mBufferResourceViews[srvFormat] = BufferSRVPair(buffer, bufferSRV);

    return bufferSRV;
}

gl::Error Buffer11::packPixels(ID3D11Texture2D *srcTexture, UINT srcSubresource, const PackPixelsParams &params)
{
    PackStorage *packStorage = getPackStorage();
    BufferStorage *latestStorage = getLatestBufferStorage();

    if (packStorage)
    {
        gl::Error error = packStorage->packPixels(srcTexture, srcSubresource, params);
        if (error.isError())
        {
            return error;
        }
        packStorage->setDataRevision(latestStorage ? latestStorage->getDataRevision() + 1 : 1);
    }

    return gl::Error(GL_NO_ERROR);
}

Buffer11::BufferStorage *Buffer11::getBufferStorage(BufferUsage usage)
{
    BufferStorage *newStorage = NULL;
    auto directBufferIt = mBufferStorages.find(usage);
    if (directBufferIt != mBufferStorages.end())
    {
        newStorage = directBufferIt->second;
    }

    if (!newStorage)
    {
        if (usage == BUFFER_USAGE_PIXEL_PACK)
        {
            newStorage = new PackStorage(mRenderer);
        }
        else if (usage == BUFFER_USAGE_SYSTEM_MEMORY)
        {
            newStorage = new SystemMemoryStorage(mRenderer);
            mHasSystemMemoryStorage = true;
        }
        else
        {
            // buffer is not allocated, create it
            newStorage = new NativeStorage(mRenderer, usage);
        }

        mBufferStorages.insert(std::make_pair(usage, newStorage));
    }

    // resize buffer
    if (newStorage->getSize() < mSize)
    {
        if (newStorage->resize(mSize, true).isError())
        {
            // Out of memory error
            return NULL;
        }
    }

    BufferStorage *latestBuffer = getLatestBufferStorage();
    if (latestBuffer && latestBuffer->getDataRevision() > newStorage->getDataRevision())
    {
        // Copy through a staging buffer if we're copying from or to a non-staging, mappable
        // buffer storage. This is because we can't map a GPU buffer, and copy CPU
        // data directly. If we're already using a staging buffer we're fine.
        if (latestBuffer->getUsage() != BUFFER_USAGE_STAGING &&
            newStorage->getUsage() != BUFFER_USAGE_STAGING &&
            (!latestBuffer->isMappable() || !newStorage->isMappable()))
        {
            NativeStorage *stagingBuffer = getStagingStorage();

            stagingBuffer->copyFromStorage(latestBuffer, 0, latestBuffer->getSize(), 0);
            stagingBuffer->setDataRevision(latestBuffer->getDataRevision());

            latestBuffer = stagingBuffer;
        }

        // if copyFromStorage returns true, the D3D buffer has been recreated
        // and we should update our serial
        if (newStorage->copyFromStorage(latestBuffer, 0, latestBuffer->getSize(), 0))
        {
            updateSerial();
        }
        newStorage->setDataRevision(latestBuffer->getDataRevision());
    }

    return newStorage;
}

Buffer11::BufferStorage *Buffer11::getLatestBufferStorage() const
{
    // Even though we iterate over all the direct buffers, it is expected that only
    // 1 or 2 will be present.
    BufferStorage *latestStorage = NULL;
    DataRevision latestRevision = 0;
    for (auto it = mBufferStorages.begin(); it != mBufferStorages.end(); it++)
    {
        BufferStorage *storage = it->second;
        if (!latestStorage || storage->getDataRevision() > latestRevision)
        {
            latestStorage = storage;
            latestRevision = storage->getDataRevision();
        }
    }

    // resize buffer
    if (latestStorage && latestStorage->getSize() < mSize)
    {
        if (latestStorage->resize(mSize, true).isError())
        {
            // Out of memory error
            return NULL;
        }
    }

    return latestStorage;
}

Buffer11::NativeStorage *Buffer11::getStagingStorage()
{
    BufferStorage *stagingStorage = getBufferStorage(BUFFER_USAGE_STAGING);

    if (!stagingStorage)
    {
        // Out-of-memory
        return NULL;
    }

    ASSERT(HAS_DYNAMIC_TYPE(NativeStorage*, stagingStorage));
    return static_cast<NativeStorage*>(stagingStorage);
}

Buffer11::PackStorage *Buffer11::getPackStorage()
{
    BufferStorage *packStorage = getBufferStorage(BUFFER_USAGE_PIXEL_PACK);

    if (!packStorage)
    {
        // Out-of-memory
        return NULL;
    }

    ASSERT(HAS_DYNAMIC_TYPE(PackStorage*, packStorage));
    return static_cast<PackStorage*>(packStorage);
}

bool Buffer11::supportsDirectBinding() const
{
    // Do not support direct buffers for dynamic data. The streaming buffer
    // offers better performance for data which changes every frame.
    // Check for absence of static buffer interfaces to detect dynamic data.
    return (mStaticVertexBuffer && mStaticIndexBuffer);
}

Buffer11::BufferStorage::BufferStorage(Renderer11 *renderer, BufferUsage usage)
    : mRenderer(renderer),
      mUsage(usage),
      mRevision(0),
      mBufferSize(0)
{
}

gl::Error Buffer11::BufferStorage::setData(const uint8_t *data, size_t offset, size_t size)
{
    ASSERT(isMappable());

    uint8_t *writePointer = map(offset, size, GL_MAP_WRITE_BIT);
    if (!writePointer)
    {
        return gl::Error(GL_OUT_OF_MEMORY, "Failed to map internal buffer.");
    }

    memcpy(writePointer, data, size);

    unmap();

    return gl::Error(GL_NO_ERROR);
}

Buffer11::NativeStorage::NativeStorage(Renderer11 *renderer, BufferUsage usage)
    : BufferStorage(renderer, usage),
      mNativeStorage(NULL)
{
}

Buffer11::NativeStorage::~NativeStorage()
{
    SafeRelease(mNativeStorage);
}

// Returns true if it recreates the direct buffer
bool Buffer11::NativeStorage::copyFromStorage(BufferStorage *source, size_t sourceOffset,
                                               size_t size, size_t destOffset)
{
    ID3D11DeviceContext *context = mRenderer->getDeviceContext();

    size_t requiredSize = sourceOffset + size;
    bool createBuffer = !mNativeStorage || mBufferSize < requiredSize;

    // (Re)initialize D3D buffer if needed
    if (createBuffer)
    {
        bool preserveData = (destOffset > 0);
        resize(source->getSize(), preserveData);
    }

    if (source->getUsage() == BUFFER_USAGE_PIXEL_PACK ||
        source->getUsage() == BUFFER_USAGE_SYSTEM_MEMORY)
    {
        ASSERT(source->isMappable());

        uint8_t *sourcePointer = source->map(sourceOffset, size, GL_MAP_READ_BIT);

        D3D11_MAPPED_SUBRESOURCE mappedResource;
        HRESULT hr = context->Map(mNativeStorage, 0, D3D11_MAP_WRITE, 0, &mappedResource);
        UNUSED_ASSERTION_VARIABLE(hr);
        ASSERT(SUCCEEDED(hr));

        uint8_t *destPointer = static_cast<uint8_t *>(mappedResource.pData) + destOffset;

        // Offset bounds are validated at the API layer
        ASSERT(sourceOffset + size <= destOffset + mBufferSize);
        memcpy(destPointer, sourcePointer, size);

        context->Unmap(mNativeStorage, 0);
        source->unmap();
    }
    else
    {
        ASSERT(HAS_DYNAMIC_TYPE(NativeStorage*, source));

        D3D11_BOX srcBox;
        srcBox.left = sourceOffset;
        srcBox.right = sourceOffset + size;
        srcBox.top = 0;
        srcBox.bottom = 1;
        srcBox.front = 0;
        srcBox.back = 1;

        ASSERT(HAS_DYNAMIC_TYPE(NativeStorage*, source));
        ID3D11Buffer *sourceBuffer = static_cast<NativeStorage*>(source)->getNativeStorage();

        context->CopySubresourceRegion(mNativeStorage, 0, destOffset, 0, 0, sourceBuffer, 0, &srcBox);
    }

    return createBuffer;
}

gl::Error Buffer11::NativeStorage::resize(size_t size, bool preserveData)
{
    ID3D11Device *device = mRenderer->getDevice();
    ID3D11DeviceContext *context = mRenderer->getDeviceContext();

    D3D11_BUFFER_DESC bufferDesc;
    fillBufferDesc(&bufferDesc, mRenderer, mUsage, size);

    ID3D11Buffer *newBuffer;
    HRESULT result = device->CreateBuffer(&bufferDesc, NULL, &newBuffer);

    if (FAILED(result))
    {
        return gl::Error(GL_OUT_OF_MEMORY, "Failed to create internal buffer, result: 0x%X.", result);
    }

    if (mNativeStorage && preserveData)
    {
        // We don't call resize if the buffer is big enough already.
        ASSERT(mBufferSize <= size);

        D3D11_BOX srcBox;
        srcBox.left = 0;
        srcBox.right = mBufferSize;
        srcBox.top = 0;
        srcBox.bottom = 1;
        srcBox.front = 0;
        srcBox.back = 1;

        context->CopySubresourceRegion(newBuffer, 0, 0, 0, 0, mNativeStorage, 0, &srcBox);
    }

    // No longer need the old buffer
    SafeRelease(mNativeStorage);
    mNativeStorage = newBuffer;

    mBufferSize = bufferDesc.ByteWidth;

    return gl::Error(GL_NO_ERROR);
}

void Buffer11::NativeStorage::fillBufferDesc(D3D11_BUFFER_DESC* bufferDesc, Renderer11 *renderer,
                                                     BufferUsage usage, unsigned int bufferSize)
{
    bufferDesc->ByteWidth = bufferSize;
    bufferDesc->MiscFlags = 0;
    bufferDesc->StructureByteStride = 0;

    switch (usage)
    {
      case BUFFER_USAGE_STAGING:
        bufferDesc->Usage = D3D11_USAGE_STAGING;
        bufferDesc->BindFlags = 0;
        bufferDesc->CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
        break;

      case BUFFER_USAGE_VERTEX_OR_TRANSFORM_FEEDBACK:
        bufferDesc->Usage = D3D11_USAGE_DEFAULT;
        bufferDesc->BindFlags = D3D11_BIND_VERTEX_BUFFER;

        if (renderer->isES3Capable())
        {
            bufferDesc->BindFlags |= D3D11_BIND_STREAM_OUTPUT;
        }

        bufferDesc->CPUAccessFlags = 0;
        break;

      case BUFFER_USAGE_INDEX:
        bufferDesc->Usage = D3D11_USAGE_DEFAULT;
        bufferDesc->BindFlags = D3D11_BIND_INDEX_BUFFER;
        bufferDesc->CPUAccessFlags = 0;
        break;

      case BUFFER_USAGE_PIXEL_UNPACK:
        bufferDesc->Usage = D3D11_USAGE_DEFAULT;
        bufferDesc->BindFlags = D3D11_BIND_SHADER_RESOURCE;
        bufferDesc->CPUAccessFlags = 0;
        break;

      case BUFFER_USAGE_UNIFORM:
        bufferDesc->Usage = D3D11_USAGE_DYNAMIC;
        bufferDesc->BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bufferDesc->CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        // Constant buffers must be of a limited size, and aligned to 16 byte boundaries
        // For our purposes we ignore any buffer data past the maximum constant buffer size
        bufferDesc->ByteWidth = roundUp(bufferDesc->ByteWidth, 16u);
        bufferDesc->ByteWidth = std::min<UINT>(bufferDesc->ByteWidth, renderer->getRendererCaps().maxUniformBlockSize);
        break;

      default:
        UNREACHABLE();
    }
}

uint8_t *Buffer11::NativeStorage::map(size_t offset, size_t length, GLbitfield access)
{
    ASSERT(mUsage == BUFFER_USAGE_STAGING);

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    ID3D11DeviceContext *context = mRenderer->getDeviceContext();
    D3D11_MAP d3dMapType = gl_d3d11::GetD3DMapTypeFromBits(access);
    UINT d3dMapFlag = ((access & GL_MAP_UNSYNCHRONIZED_BIT) != 0 ? D3D11_MAP_FLAG_DO_NOT_WAIT : 0);

    HRESULT result = context->Map(mNativeStorage, 0, d3dMapType, d3dMapFlag, &mappedResource);
    UNUSED_ASSERTION_VARIABLE(result);
    ASSERT(SUCCEEDED(result));

    return static_cast<uint8_t*>(mappedResource.pData) + offset;
}

void Buffer11::NativeStorage::unmap()
{
    ASSERT(mUsage == BUFFER_USAGE_STAGING);
    ID3D11DeviceContext *context = mRenderer->getDeviceContext();
    context->Unmap(mNativeStorage, 0);
}

Buffer11::PackStorage::PackStorage(Renderer11 *renderer)
    : BufferStorage(renderer, BUFFER_USAGE_PIXEL_PACK),
      mStagingTexture(NULL),
      mTextureFormat(DXGI_FORMAT_UNKNOWN),
      mQueuedPackCommand(NULL),
      mDataModified(false)
{
}

Buffer11::PackStorage::~PackStorage()
{
    SafeRelease(mStagingTexture);
    SafeDelete(mQueuedPackCommand);
}

bool Buffer11::PackStorage::copyFromStorage(BufferStorage *source, size_t sourceOffset,
                                              size_t size, size_t destOffset)
{
    // We copy through a staging buffer when drawing with a pack buffer,
    // or for other cases where we access the pack buffer
    UNREACHABLE();
    return false;
}

gl::Error Buffer11::PackStorage::resize(size_t size, bool preserveData)
{
    if (size != mBufferSize)
    {
        if (!mMemoryBuffer.resize(size))
        {
            return gl::Error(GL_OUT_OF_MEMORY, "Failed to resize internal buffer storage.");
        }
        mBufferSize = size;
    }

    return gl::Error(GL_NO_ERROR);
}

uint8_t *Buffer11::PackStorage::map(size_t offset, size_t length, GLbitfield access)
{
    ASSERT(offset + length <= getSize());
    // TODO: fast path
    //  We might be able to optimize out one or more memcpy calls by detecting when
    //  and if D3D packs the staging texture memory identically to how we would fill
    //  the pack buffer according to the current pack state.

    gl::Error error = flushQueuedPackCommand();
    if (error.isError())
    {
        return NULL;
    }

    mDataModified = (mDataModified || (access & GL_MAP_WRITE_BIT) != 0);

    return mMemoryBuffer.data() + offset;
}

void Buffer11::PackStorage::unmap()
{
    // No-op
}

gl::Error Buffer11::PackStorage::packPixels(ID3D11Texture2D *srcTexure, UINT srcSubresource, const PackPixelsParams &params)
{
    gl::Error error = flushQueuedPackCommand();
    if (error.isError())
    {
        return error;
    }

    mQueuedPackCommand = new PackPixelsParams(params);

    D3D11_TEXTURE2D_DESC textureDesc;
    srcTexure->GetDesc(&textureDesc);

    if (mStagingTexture != NULL &&
        (mTextureFormat != textureDesc.Format ||
         mTextureSize.width != params.area.width ||
         mTextureSize.height != params.area.height))
    {
        SafeRelease(mStagingTexture);
        mTextureSize.width = 0;
        mTextureSize.height = 0;
        mTextureFormat = DXGI_FORMAT_UNKNOWN;
    }

    if (mStagingTexture == NULL)
    {
        ID3D11Device *device = mRenderer->getDevice();
        HRESULT hr;

        mTextureSize.width = params.area.width;
        mTextureSize.height = params.area.height;
        mTextureFormat = textureDesc.Format;

        D3D11_TEXTURE2D_DESC stagingDesc;
        stagingDesc.Width = params.area.width;
        stagingDesc.Height = params.area.height;
        stagingDesc.MipLevels = 1;
        stagingDesc.ArraySize = 1;
        stagingDesc.Format = mTextureFormat;
        stagingDesc.SampleDesc.Count = 1;
        stagingDesc.SampleDesc.Quality = 0;
        stagingDesc.Usage = D3D11_USAGE_STAGING;
        stagingDesc.BindFlags = 0;
        stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        stagingDesc.MiscFlags = 0;

        hr = device->CreateTexture2D(&stagingDesc, NULL, &mStagingTexture);
        if (FAILED(hr))
        {
            ASSERT(hr == E_OUTOFMEMORY);
            return gl::Error(GL_OUT_OF_MEMORY, "Failed to allocate internal staging texture.");
        }
    }

    // ReadPixels from multisampled FBOs isn't supported in current GL
    ASSERT(textureDesc.SampleDesc.Count <= 1);

    ID3D11DeviceContext *immediateContext = mRenderer->getDeviceContext();
    D3D11_BOX srcBox;
    srcBox.left   = params.area.x;
    srcBox.right  = params.area.x + params.area.width;
    srcBox.top    = params.area.y;
    srcBox.bottom = params.area.y + params.area.height;
    srcBox.front  = 0;
    srcBox.back   = 1;

    // Asynchronous copy
    immediateContext->CopySubresourceRegion(mStagingTexture, 0, 0, 0, 0, srcTexure, srcSubresource, &srcBox);

    return gl::Error(GL_NO_ERROR);
}

gl::Error Buffer11::PackStorage::flushQueuedPackCommand()
{
    ASSERT(mMemoryBuffer.size() > 0);

    if (mQueuedPackCommand)
    {
        gl::Error error = mRenderer->packPixels(mStagingTexture, *mQueuedPackCommand, mMemoryBuffer.data());
        SafeDelete(mQueuedPackCommand);
        if (error.isError())
        {
            return error;
        }
    }

    return gl::Error(GL_NO_ERROR);
}

Buffer11::SystemMemoryStorage::SystemMemoryStorage(Renderer11 *renderer)
    : Buffer11::BufferStorage(renderer, BUFFER_USAGE_SYSTEM_MEMORY)
{}

bool Buffer11::SystemMemoryStorage::copyFromStorage(BufferStorage *source, size_t sourceOffset,
                                                    size_t size, size_t destOffset)
{
    ASSERT(source->isMappable());
    const uint8_t *sourceData = source->map(sourceOffset, size, GL_MAP_READ_BIT);
    ASSERT(destOffset + size <= mSystemCopy.size());
    memcpy(mSystemCopy.data() + destOffset, sourceData, size);
    source->unmap();
    return true;
}

gl::Error Buffer11::SystemMemoryStorage::resize(size_t size, bool preserveData)
{
    if (mSystemCopy.size() < size)
    {
        if (!mSystemCopy.resize(size))
        {
            return gl::Error(GL_OUT_OF_MEMORY, "Failed to resize SystemMemoryStorage");
        }
        mBufferSize = size;
    }

    return gl::Error(GL_NO_ERROR);
}

uint8_t *Buffer11::SystemMemoryStorage::map(size_t offset, size_t length, GLbitfield access)
{
    ASSERT(!mSystemCopy.empty() && offset + length <= mSystemCopy.size());
    return mSystemCopy.data() + offset;
}

void Buffer11::SystemMemoryStorage::unmap()
{
    // No-op
}

}
