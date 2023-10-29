#pragma once

#include "datasource.hpp"

#include "ntdll64.hpp"

class ReadOnlyMemoryDataSource : public ReadOnlyDataSource
{
public:
    ReadOnlyMemoryDataSource(HANDLE hProcess, uint64_t baseAddress, uint64_t size);
    uint64_t GetBaseAddress() const noexcept { return mBaseAddress; }

protected:
    virtual size_t ReadImpl(void* buffer, size_t bufferLength) override;
    virtual void SeekImpl(uint64_t newOffset) override;
    virtual uint64_t GetSizeImpl() override { return mSize; }

    uint64_t  mBaseAddress;
    uint64_t  mSize;
    uint64_t  mOffset;
    HANDLE    mProcess;

    const IWow64Helper& mApi;
};

class ReadOnlyMemoryDataSourceEx : protected ReadOnlyMemoryDataSource
{
public:
    ReadOnlyMemoryDataSourceEx(DWORD pid, uint64_t baseAddress, uint64_t size);

    ~ReadOnlyMemoryDataSourceEx()
    {
        if (mProcess != nullptr)
            CloseHandle(mProcess);
    }

    size_t ReadImpl(void* buffer, size_t bufferLength) override { return ReadOnlyMemoryDataSource::ReadImpl(buffer, bufferLength); }
    void SeekImpl(uint64_t newOffset) override { ReadOnlyMemoryDataSource::SeekImpl(newOffset); }
    uint64_t GetSizeImpl() override { return ReadOnlyMemoryDataSource::GetSizeImpl(); }
};
