#include "file.hpp"
#include "memdatasource.hpp"
#include "memhelper.hpp"
#include "pe.hpp"

template <CPUArchitecture arch>
int CheckPE(std::shared_ptr<DataSource> mapped)
{
	try
	{
		auto imagePath = GetMemoryHelper().GetImageNameByAddress(GetCurrentProcess(), mapped->GetOrigin());
		PE<false, arch> imageOnDisk(std::make_shared<File>(imagePath.c_str()));
		
		auto result = imageOnDisk.CheckExportForHooks(mapped);

		return result.size() == 1 ? 0 : 11; // can fail if there are unexpected hooks
	}
	catch (const PeException&)
	{
		return 10;
	}
}

int main()
{
	auto moduleHandle = GetModuleHandleW(L"kernelbase");
	if (moduleHandle == nullptr)
		return 1;

	auto ptr = (uint8_t*)GetProcAddress(moduleHandle, "EnumDeviceDrivers");
	DWORD oldProt = 0;
	if (!VirtualProtect(ptr, 0x1000, PAGE_EXECUTE_READWRITE, &oldProt))
		return 4;

	*ptr = 0xe9;

	ReadOnlyMemoryDataSource moduleMapped(GetCurrentProcess(), (uintptr_t)moduleHandle - 0x1000, 100 * 1024 * 1024);
	return CheckPE<CURRENT_MODULE_ARCH>(std::make_shared<DataSourceFragment>(moduleMapped, 0x1000, 50 * 1024 * 1024));
}
