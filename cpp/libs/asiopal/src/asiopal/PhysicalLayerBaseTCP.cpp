/**
 * Licensed to Green Energy Corp (www.greenenergycorp.com) under one or
 * more contributor license agreements. See the NOTICE file distributed
 * with this work for additional information regarding copyright ownership.
 * Green Energy Corp licenses this file to you under the Apache License,
 * Version 2.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * This project was forked on 01/01/2013 by Automatak, LLC and modifications
 * may have been made to this file. Automatak, LLC licenses these modifications
 * to you under the terms of the License.
 */
#include "PhysicalLayerBaseTCP.h"

#include <string>
#include <functional>

#include <asio.hpp>
#include <asio/ip/tcp.hpp>

#include <openpal/logging/LogMacros.h>
#include <openpal/channel/IPhysicalLayerCallbacks.h>
#include <openpal/logging/LogLevels.h>

using namespace asio;

using namespace std;
using namespace openpal;

namespace asiopal
{

PhysicalLayerBaseTCP::PhysicalLayerBaseTCP(openpal::LogRoot& root, asio::io_service* apIOService) :
	PhysicalLayerASIO(root, apIOService),
	mSocket(*apIOService)
{
	
}

/* Implement the actions */

void PhysicalLayerBaseTCP::DoClose()
{
	this->ShutdownSocket();
	this->CloseSocket();
}

void PhysicalLayerBaseTCP::DoRead(WriteBuffer& buff)
{
	uint8_t* pBuff = buff;
	mSocket.async_read_some(buffer(pBuff, buff.Size()),
	                        strand.wrap([this, pBuff](const std::error_code & code, size_t  numRead)
	{
		this->OnReadCallback(code, pBuff, static_cast<uint32_t>(numRead));
	}));
}

void PhysicalLayerBaseTCP::DoWrite(const ReadOnlyBuffer& buff)
{
	async_write(mSocket, buffer(buff, buff.Size()),
	            strand.wrap([this](const std::error_code & code, size_t  numWritten)
	{
		this->OnWriteCallback(code, static_cast<uint32_t>(numWritten));
	}));
}

void PhysicalLayerBaseTCP::DoOpenFailure()
{
	SIMPLE_LOG_BLOCK(logger, logflags::DBG, "Failed socket open, closing socket");
	this->CloseSocket();
}

void PhysicalLayerBaseTCP::CloseSocket()
{
	std::error_code ec;

	mSocket.close(ec);
	if (ec)
	{
		FORMAT_LOG_BLOCK(logger, logflags::WARN, "Error while closing socket: %s", ec.message().c_str());
	}
}

void PhysicalLayerBaseTCP::ShutdownSocket()
{
	std::error_code ec;

	mSocket.shutdown(ip::tcp::socket::shutdown_both, ec);
	if (ec)
	{
		FORMAT_LOG_BLOCK(logger, logflags::WARN, "Error while shutting down socket: %s", ec.message().c_str());
	}
}

}


