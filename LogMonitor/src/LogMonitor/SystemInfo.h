#pragma once
#ifndef _SystemInfo_h
#define _SystemInfo_h

#include "pch.h"

class SystemInfo
{
public:
	LPCWSTR Platform;

	LPCWSTR NetBIOS;
	LPCWSTR DnsHostname;
	LPCWSTR DnsDomain;
	LPCWSTR DnsFullyQualified;
	LPCWSTR PhysicalNetBIOS;
	LPCWSTR PhysicalDnsHostname;
	LPCWSTR PhysicalDnsDomain;
	LPCWSTR PhysicalDnsFullyQualified;

	std::string SystemVersion;

	bool ISWindowsServer;

public:
	SystemInfo() 
	{
		SYSTEM_INFO systemInfo;
		GetSystemInfo(&systemInfo);
		switch (systemInfo.wProcessorArchitecture)
		{
		case PROCESSOR_ARCHITECTURE_AMD64:this->Platform = (L"x64 (AMD or Intel)");
			break;
		case PROCESSOR_ARCHITECTURE_ARM:this->Platform = (L"ARM");
			break;
		case PROCESSOR_ARCHITECTURE_ARM64:this->Platform = (L"ARM64");
			break;
		case PROCESSOR_ARCHITECTURE_IA64:this->Platform = (L"Intel Itanium-based");
			break;
		case PROCESSOR_ARCHITECTURE_INTEL:this->Platform = (L"x86");
			break;
		case PROCESSOR_ARCHITECTURE_MIPS:this->Platform = (L"MIPS");
			break;
		case PROCESSOR_ARCHITECTURE_ALPHA:this->Platform = (L"ALPHA");
			break;
		case PROCESSOR_ARCHITECTURE_PPC:this->Platform = (L"PPC");
			break;
		case PROCESSOR_ARCHITECTURE_UNKNOWN:this->Platform = (L"Unknown architecture");
			break;
		}

		TCHAR buffer[MAX_COMPUTERNAME_LENGTH + 1];
		DWORD dwSize = _countof(buffer);
		int cnf = 0;

		for (cnf = 0; cnf < ComputerNameMax; cnf++) 
		{
			if (!GetComputerNameEx((COMPUTER_NAME_FORMAT)cnf, buffer, &dwSize))
			{
				return;
			} 
			else
			{
				switch (cnf)
				{
				case 0: this->NetBIOS = buffer;
					break;
				case 1: this->DnsHostname = buffer;
					break;
				case 2: this->DnsDomain = buffer;
					break;
				case 3: this->DnsFullyQualified = buffer;
					break;
				case 4: this->PhysicalNetBIOS = buffer;
					break;
				case 5: this->PhysicalDnsHostname = buffer;
					break;
				case 6: this->PhysicalDnsDomain = buffer;
					break;
				case 7: this->PhysicalDnsFullyQualified = buffer;
					break;
				default:
					break;
				}
			}

			dwSize = _countof(buffer);
			ZeroMemory(buffer, dwSize);
		}

		
		if (IsWindowsXPOrGreater())
		{
			this->SystemVersion = "XPOrGreater";
		}

		if (IsWindowsXPSP1OrGreater())
		{
			this->SystemVersion = "XPSP1OrGreater";
		}

		if (IsWindowsXPSP2OrGreater())
		{
			this->SystemVersion = "XPSP2OrGreater";
		}

		if (IsWindowsXPSP3OrGreater())
		{
			this->SystemVersion = "XPSP3OrGreater";
		}

		if (IsWindowsVistaOrGreater())
		{
			this->SystemVersion = "VistaOrGreater";
		}

		if (IsWindowsVistaSP1OrGreater())
		{
			this->SystemVersion = "VistaSP1OrGreater";
		}

		if (IsWindowsVistaSP2OrGreater())
		{
			this->SystemVersion = "VistaSP2OrGreater";
		}

		if (IsWindows7OrGreater())
		{
			this->SystemVersion = "Windows7OrGreater";
		}

		if (IsWindows7SP1OrGreater())
		{
			this->SystemVersion = "Windows7SP1OrGreater";
		}

		if (IsWindows8OrGreater())
		{
			this->SystemVersion = "Windows8OrGreater";
		}

		if (IsWindows8Point1OrGreater())
		{
			this->SystemVersion = "Windows8Point1OrGreater";
		}

		if (IsWindows10OrGreater())
		{
			this->SystemVersion = "Windows10OrGreater";
		}

		this->ISWindowsServer = IsWindowsServer();
		
	}

	~SystemInfo(){}
};

#endif _SystemInfo_h
