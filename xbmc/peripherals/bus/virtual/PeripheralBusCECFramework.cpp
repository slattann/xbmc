/*
 *      Copyright (C) 2017 Gerald Dachs
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

extern "C" {
#include <linux/cec.h>
}

#include "PeripheralBusCECFramework.h"
#include "peripherals/Peripherals.h"


static const int maxProbedCECDevices = 8;

using namespace PERIPHERALS;

CPeripheralBusCECFramework::CPeripheralBusCECFramework(CPeripherals& manager) :
    CPeripheralBus("PeripBusCECFramework", manager, PERIPHERAL_BUS_CEC_FRAMEWORK)
{
  m_iRescanTime = 5000;
}

CPeripheralBusCECFramework::~CPeripheralBusCECFramework(void)
{
}

bool CPeripheralBusCECFramework::PerformDeviceScan(PeripheralScanResults &results)
{
  for (uint8_t iDevice = 0; iDevice < maxProbedCECDevices; iDevice++)
  {
		std::string strComName("/dev/cec");
		strComName += std::to_string(iDevice);
		auto fd = open(strComName.c_str(), O_RDWR);
		if (fd < 0)
		{
			continue;
		}

		struct cec_caps caps = { };

		auto ret = ioctl(fd, CEC_ADAP_G_CAPS, &caps);
		if (ret < 0)
		{
			return false;
		}

		struct cec_log_addrs cec_log_addrs = { };

		ret = ioctl(fd, CEC_ADAP_G_LOG_ADDRS, &cec_log_addrs);
		if (ret < 0)
		{
			return false;
		}

		PeripheralScanResult result(m_type);
		result.m_iVendorId   = cec_log_addrs.vendor_id;
		result.m_iProductId  = 0;
		result.m_strLocation = strComName;
		result.m_type = PERIPHERAL_CEC_FRAMEWORK;

		result.m_iSequence = GetNumberOfPeripheralsWithId(result.m_iVendorId, result.m_iProductId);
		if (!results.ContainsResult(result))
			results.m_results.push_back(result)

		close(fd);
	}
  return true;
}
