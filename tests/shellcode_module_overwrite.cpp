#include <set>

#include "callbacks.hpp"
#include "file.hpp"
#include "memdatasource.hpp"
#include "memhelper.hpp"
#include "pe.hpp"
#include "yara.hpp"

const char gShellcode[] =
"\xC3\xFC\xE8\x89\x00\x00\x00\x60\x89\xE5\x31\xD2\x64\x8B\x52\x30\x8B\x52\x0C\x8B\x52\x14\x8B\x72\x28\x0F\xB7\x4A\x26\x31\xFF\x31\xC0\xAC\x3C\x61\x7C\x02\x2C\x20\xC1\xCF\x0D\x01\xC7\xE2\xF0"
"\x52\x57\x8B\x52\x10\x8B\x42\x3C\x01\xD0\x8B\x40\x78\x85\xC0\x74\x4A\x01\xD0\x50\x8B\x48\x18\x8B\x58\x20\x01\xD3\xE3\x3C\x49\x8B\x34\x8B\x01\xD6\x31\xFF\x31\xC0\xAC\xC1\xCF\x0D\x01\xC7"
"\x38\xE0\x75\xF4\x03\x7D\xF8\x3B\x7D\x24\x75\xE2\x58\x8B\x58\x24\x01\xD3\x66\x8B\x0C\x4B\x8B\x58\x1C\x01\xD3\x8B\x04\x8B\x01\xD0\x89\x44\x24\x24\x5B\x5B\x61\x59\x5A\x51\xFF\xE0\x58\x5F"
"\x5A\x8B\x12\xEB\x86\x5D\x68\x33\x32\x00\x00\x68\x77\x73\x32\x5F\x54\x68\x4C\x77\x26\x07\xFF\xD5\xB8\x90\x01\x00\x00\x29\xC4\x54\x50\x68\x29\x80\x6B\x00\xFF\xD5\x50\x50\x50\x50\x40\x50"
"\x40\x50\x68\xEA\x0F\xDF\xE0\xFF\xD5\x89\xC7\x31\xDB\x53\x68\x02\x00\x11\x5C\x89\xE6\x6A\x10\x56\x57\x68\xC2\xDB\x37\x67\xFF\xD5\x53\x57\x68\xB7\xE9\x38\xFF\xFF\xD5\x53\x53\x57\x68\x74"
"\xEC\x3B\xE1\xFF\xD5\x57\x89\xC7\x68\x75\x6E\x4D\x61\xFF\xD5\x68\x63\x6D\x64\x00\x89\xE3\x57\x57\x57\x31\xF6\x6A\x12\x59\x56\xE2\xFD\x66\xC7\x44\x24\x3C\x01\x01\x8D\x44\x24\x10\xC6\x00"
"\x44\x54\x50\x56\x56\x56\x46\x56\x4E\x56\x56\x53\x56\x68\x79\xCC\x3F\x86\xFF\xD5\x89\xE0\x4E\x56\x46\xFF\x30\x68\x08\x87\x1D\x60\xFF\xD5\xBB\xF0\xB5\xA2\x56\x68\xA6\x95\xBD\x9D\xFF\xD5"
"\x3C\x06\x7C\x0A\x80\xFB\xE0\x75\x05\xBB\x47\x13\x72\x6F\x6A\x00\x53\xFF\xD5";

class MyCallbacks : public DefaultCallbacks
{
public:
    void OnSuspiciousMemoryRegionFound(const MemoryHelperBase::FlatMemoryMapT& continiousRegions,
        const std::vector<uint64_t>& threadEntryPoints, bool& scanWithYara) override
    {
        Super::OnSuspiciousMemoryRegionFound(continiousRegions, threadEntryPoints, scanWithYara);
    }

	void OnYaraScan(const MemoryHelperBase::MemInfoT64& region, uint64_t startAddress, uint64_t size, bool externalOperation, 
		OperationType operation, bool isAlignedAllocation, const std::set<std::string>* detections) override
	{
		Super::OnYaraScan(region, startAddress, size, externalOperation, operation, isAlignedAllocation, detections);
		if (detections)
			mYaraDetections.insert(detections->begin(), detections->end());
	}

	void OnPrivateCodeModification(const wchar_t* imageName, uint64_t imageBase, uint32_t rva,
		const AddressInfo* addressInfo = nullptr) override
	{
		Super::OnPrivateCodeModification(imageName, imageBase, rva, addressInfo);
		mOverwriteRva = rva;
	}

	MyCallbacks(void* address, uint64_t size) : 
		DefaultCallbacks({ GetCurrentProcessId(), (uintptr_t)address, size, true, OperationType::Write }, DefaultCallbacks::ScanningGeneralSettings{}), mOverwriteRva(0)
    {}

	const std::set<std::string>& GetYaraDetections() const noexcept { return mYaraDetections; }
	uint32_t GetOverwriteRva() const noexcept { return mOverwriteRva; }

private:
    typedef DefaultCallbacks Super;
	std::set<std::string> mYaraDetections;
	uint32_t mOverwriteRva;
};


int main()
{
	auto randomLib = LoadLibraryW(L"dbghelp.dll");
	PE<true, CURRENT_MODULE_ARCH> pe(std::make_shared<ReadOnlyMemoryDataSource>(GetCurrentProcess(), (uintptr_t)randomLib, 100 * 1024 * 1024));

	DWORD oldProt;
	auto addressToWrite = (char*)randomLib + pe.GetEntryPointRVA();
	VirtualProtect(addressToWrite, sizeof(gShellcode), PAGE_EXECUTE_READWRITE, &oldProt);
	memcpy(addressToWrite, gShellcode, sizeof(gShellcode));
	VirtualProtect(addressToWrite, sizeof(gShellcode), oldProt, &oldProt);

	auto callbacks = std::make_shared<MyCallbacks>(addressToWrite, sizeof(gShellcode));
	auto& scanner = MemoryScanner::GetInstance();
	scanner.SetYaraRules(std::make_shared<YaraScanner::YaraRules>(YARA_RULES_DIR));
	MemoryScanner::GetInstance().Scan(callbacks);
	const auto& detections = callbacks->GetYaraDetections();

	bool shellcodeFound = detections.find("GenericShellcode32") != detections.end(),
		moduleOverwriteDetected = callbacks->GetOverwriteRva() == pe.GetEntryPointRVA();

	TerminateProcess(GetCurrentProcess(), shellcodeFound && moduleOverwriteDetected ? 0 : 1);
}