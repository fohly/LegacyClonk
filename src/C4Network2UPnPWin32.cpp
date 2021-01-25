/*
 * LegacyClonk
 *
 * Copyright (c) 2020, The LegacyClonk Team and contributors
 *
 * Distributed under the terms of the ISC license; see accompanying file
 * "COPYING" for details.
 *
 * "Clonk" is a registered trademark of Matthes Bender, used with permission.
 * See accompanying file "TRADEMARK" for details.
 *
 * To redistribute this file separately, substitute the full license texts
 * for the above references.
 */

#pragma once

#include "C4Network2UPnPWin32.h"
#include "C4Version.h"
#include "StdResStr2.h"
#include "StdStringEncodingConverter.h"

#include "comdef.h"

namespace
{
	template<typename T> void SafeRelease(T *const &ptr)
	{
		if (ptr)
		{
			ptr->Release();
		}
	}

	class OLEString
	{
	public:
		OLEString(const OLECHAR *const string) : string{SysAllocString(string)} {}
		~OLEString() { SysFreeString(string); }

	public:
		operator BSTR() const { return string; }

	private:
		BSTR string;
	};

	static const OLEString PROTO_TCP{L"TCP"};
	static const OLEString PROTO_UDP{L"UDP"};
}

C4Network2UPnPImplWin32::C4Network2UPnPImplWin32() : C4Network2UPnPImpl{}
{
	IUPnPNAT *nat{nullptr};
	if (FAILED(CoCreateInstance(CLSID_UPnPNAT, nullptr, CLSCTX_INPROC_SERVER, IID_IUPnPNAT, reinterpret_cast<LPVOID *>(&nat))))
	{
		throw std::runtime_error{"No service"};
	}

	static constexpr size_t NUMBER_OF_TRIES{10};
	for (size_t i{0}; i < NUMBER_OF_TRIES; ++i)
	{
		if (SUCCEEDED(nat->get_StaticPortMappingCollection(&mappings)) && mappings)
		{
			LogSilentF("UPnP: Got NAT port mapping table after %zu tries", i + 1);
			break;
		}

		if (i == 2)
		{
			Log(LoadResStr("IDS_MSG_UPNPHINT"));
			Sleep(1000);
		}
	}

	SafeRelease(nat);
	if (!mappings)
	{
		throw std::runtime_error{"No mapping"};
	}
}

void C4Network2UPnPImplWin32::AddMapping(C4Network2IOProtocol protocol, std::uint16_t internalPort, std::uint16_t externalPort)
{
	if (mappings)
	{
		char hostname[MAX_PATH];

		if (gethostname(hostname, MAX_PATH) == 0)
		{
#if 0
			addrinfo hints{};
			if (C4NetAddressInfo addrs{hostname, std::to_string(internalPort).c_str(), &hints}; addrs)
			{
				const StdStringEncodingConverter converter;
				const OLEString description{converter.WinAcpToUtf16(C4ENGINECAPTION).c_str()};
				const addrinfo &address = addrs.GetAddrs()[0];

				static constexpr auto LENGTH = std::max(INET_ADDRSTRLEN, INET6_ADDRSTRLEN);
				char buffer[LENGTH + 1];
				const OLEString client{converter.WinAcpToUtf16(inet_ntop(address.ai_family, address.ai_addr, buffer, LENGTH)).c_str()};
#else
			if (hostent *host{gethostbyname(hostname)}; host)
			{
				in_addr addr;
				addr.s_addr = *reinterpret_cast<ULONG *>(host->h_addr_list[0]);

				const StdStringEncodingConverter converter;
				const OLEString description{converter.WinAcpToUtf16(C4ENGINECAPTION).c_str()};
				const OLEString client{converter.WinAcpToUtf16(inet_ntoa(addr)).c_str()};
#endif

				IStaticPortMapping *mapping{nullptr};
				if (const HRESULT result{mappings->Add(externalPort, protocol == P_TCP ? PROTO_TCP : PROTO_UDP, internalPort, client, VARIANT_TRUE, description, &mapping)}; SUCCEEDED(result))
				{
					addedMappings.insert(mapping);
					LogSilentF("UPnP: Successfully opened port %u->%s:%u (%s)", externalPort, static_cast<BSTR>(client), internalPort, protocol == P_TCP ? "TCP" : "UDP");
				}
				else
				{
					LogF("Failed to open port %u->%s:%u (%s): %s (%x)", externalPort, static_cast<BSTR>(client), internalPort, protocol == P_TCP ? "TCP" : "UDP", _com_error{result}.ErrorMessage(), result);
				}
			}
		}
	}
}

void C4Network2UPnPImplWin32::ClearMappings()
{
	if (!mappings)
	{
		return;
	}

	for (IStaticPortMapping *const mapping : addedMappings)
	{
		BSTR protocol;
		BSTR client;
		long internalPort;
		long externalPort;

		mapping->get_InternalPort(&internalPort);
		mapping->get_ExternalPort(&externalPort);
		mapping->get_InternalClient(&client);
		mapping->get_Protocol(&protocol);

		if (SUCCEEDED(mappings->Remove(externalPort, protocol)))
		{
			LogSilentF("UPnP: Closed port %ld->%s:%ld (%s)", externalPort, client, internalPort, protocol);
		}

		SysFreeString(protocol);
		SysFreeString(client);
		mapping->Release();
	}

	mappings->Release();
}
