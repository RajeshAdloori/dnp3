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
#include <catch.hpp>

#include "LinkReceiverTest.h"
#include "DNPHelpers.h"

#include <openpal/container/StaticBuffer.h>

using namespace openpal;
using namespace opendnp3;


#define SUITE(name) "LinkParserTestSuite - " name

TEST_CASE(SUITE("InitializationState"))
{
	LinkParserTest t;
	REQUIRE(t.sink.mNumFrames ==  0);
}

//////////////////////////////////////////
//  CRC Failures
//////////////////////////////////////////

TEST_CASE(SUITE("HeaderCRCError"))
{
	LinkParserTest t;
	t.WriteData("05 64 05 C0 01 00 00 04 E9 20");
	REQUIRE(t.sink.mNumFrames ==  0);
	REQUIRE(t.log.NextErrorCode() ==  DLERR_CRC);
}

TEST_CASE(SUITE("BodyCRCError"))
{
	LinkParserTest t;
	t.WriteData("05 64 14 F3 01 00 00 04 0A 3B C0 C3 01 3C 02 06 3C 03 06 3C 04 06 3C 01 06 9A 11");
	REQUIRE(t.sink.mNumFrames ==  0);
	REQUIRE(t.log.NextErrorCode() ==  DLERR_CRC);
}

//////////////////////////////////////////
//  Illegal packets of one form or another
//////////////////////////////////////////

// Change the length to something from 5 to something in [0,4] and recalc the crc
// A valid reset link states packet would be: 05 64 05 C0 01 00 00 04 E9 21
TEST_CASE(SUITE("BadLengthError"))
{
	LinkParserTest t;
	t.WriteData(RepairCRC("05 64 01 C0 01 00 00 04 E9 21"));
	REQUIRE(t.sink.mNumFrames ==  0);
	REQUIRE(t.log.NextErrorCode() ==  DLERR_INVALID_LENGTH);
}

//Test that the presence of user data disagrees with the function code
TEST_CASE(SUITE("UnexpectedData"))
{
	LinkParserTest t;
	t.WriteData(RepairCRC("05 64 08 C0 01 00 00 04 E9 21"));
	REQUIRE(t.sink.mNumFrames ==  0);
	REQUIRE(t.log.NextErrorCode() ==  DLERR_UNEXPECTED_DATA);
}

// Test that the absence of user data disagrees with the function code
// This is the first 10 bytes of an unconfirmed user data packet w/ the length set to 5
TEST_CASE(SUITE("AbsenceOfData"))
{
	LinkParserTest t;
	t.WriteData(RepairCRC("05 64 05 73 00 04 01 00 03 FC"));
	REQUIRE(t.sink.mNumFrames ==  0);
	REQUIRE(t.log.NextErrorCode() ==  DLERR_NO_DATA);
}

// Test that the parser can handle an unknown PriToSec function code
// Reset Links w/ function code changed from 0 to 6
TEST_CASE(SUITE("UnknownFunction"))
{
	LinkParserTest t;
	t.WriteData(RepairCRC("05 64 05 C6 01 00 00 04 E9 21"));
	REQUIRE(t.sink.mNumFrames ==  0);
	REQUIRE(t.log.NextErrorCode() ==   DLERR_UNKNOWN_FUNC);
}

// Test that the parser can handle an unexpected FCV bit
// Reset Links w/ FCV toggled on
TEST_CASE(SUITE("UnexpectedFCV"))
{
	LinkParserTest t;
	t.WriteData(RepairCRC("05 64 05 D0 01 00 00 04 E9 21"));
	REQUIRE(t.sink.mNumFrames ==  0);
	REQUIRE(t.log.NextErrorCode() ==  DLERR_UNEXPECTED_FCV);
}

// Test that the parser can handle an unexpected FCB bit for SecToPri
// ACK w/ FCB toggled on
TEST_CASE(SUITE("UnexpectedFCB"))
{
	LinkParserTest t;
	t.WriteData(RepairCRC("05 64 05 20 00 04 01 00 19 A6"));
	REQUIRE(t.sink.mNumFrames ==  0);
	REQUIRE(t.log.NextErrorCode() ==  DLERR_UNEXPECTED_FCB);
}

// Write two bad packets back-to-back tests that this produces
// back to back errors with a single write call
TEST_CASE(SUITE("CombinedFailures"))
{
	LinkParserTest t;
	t.WriteData(RepairCRC("05 64 05 20 00 04 01 00 19 A6")
	            + " " + RepairCRC("05 64 05 D0 01 00 00 04 E9 21"));
	REQUIRE(t.sink.mNumFrames ==  0);
	REQUIRE(t.log.NextErrorCode() ==  DLERR_UNEXPECTED_FCB);
	REQUIRE(t.log.NextErrorCode() ==  DLERR_UNEXPECTED_FCV);
}

//////////////////////////////////////////
// Successful packets - Sec to Pri
//////////////////////////////////////////

TEST_CASE(SUITE("ReadACK"))
{
	StaticBuffer<292> buffer;
	auto writeTo = buffer.GetWriteBuffer();
	auto frame = LinkFrame::FormatAck(writeTo, true, false, 1, 2, nullptr);

	LinkParserTest t;
	t.WriteData(frame);
	REQUIRE(t.log.IsLogErrorFree());
	REQUIRE(t.sink.mNumFrames ==  1);
	REQUIRE(t.sink.CheckLastWithDFC(LinkFunction::SEC_ACK, true, false, 1, 2));
}

TEST_CASE(SUITE("ReadNACK"))
{
	StaticBuffer<292> buffer;
	auto writeTo = buffer.GetWriteBuffer();
	auto frame = LinkFrame::FormatNack(writeTo, false, true, 1, 2, nullptr);

	LinkParserTest t;
	t.WriteData(frame);
	REQUIRE(t.log.IsLogErrorFree());
	REQUIRE(t.sink.mNumFrames ==  1);
	REQUIRE(t.sink.CheckLastWithDFC(LinkFunction::SEC_NACK, false, true, 1, 2));
}

TEST_CASE(SUITE("LinkStatus"))
{
	StaticBuffer<292> buffer;
	auto writeTo = buffer.GetWriteBuffer();
	auto frame = LinkFrame::FormatLinkStatus(writeTo, true, true, 1, 2, nullptr);

	LinkParserTest t;
	t.WriteData(frame);
	REQUIRE(t.log.IsLogErrorFree());
	REQUIRE(t.sink.mNumFrames ==  1);
	REQUIRE(t.sink.CheckLastWithDFC(LinkFunction::SEC_LINK_STATUS, true, true, 1, 2));
}

TEST_CASE(SUITE("NotSupported"))
{
	StaticBuffer<292> buffer;
	auto writeTo = buffer.GetWriteBuffer();
	auto frame = LinkFrame::FormatNotSupported(writeTo, true, false, 1, 2, nullptr);

	LinkParserTest t;
	t.WriteData(frame);
	REQUIRE(t.log.IsLogErrorFree());
	REQUIRE(t.sink.mNumFrames ==  1);
	REQUIRE(t.sink.CheckLastWithDFC(LinkFunction::SEC_NOT_SUPPORTED, true, false, 1, 2));
}

//////////////////////////////////////////
// Successful packets - Pri To Sec
//////////////////////////////////////////

TEST_CASE(SUITE("TestLinkStates"))
{
	StaticBuffer<292> buffer;
	auto writeTo = buffer.GetWriteBuffer();
	auto frame = LinkFrame::FormatTestLinkStatus(writeTo, false, true, 1, 2, nullptr);

	LinkParserTest t;
	t.WriteData(frame);
	REQUIRE(t.log.IsLogErrorFree());
	REQUIRE(t.sink.mNumFrames ==  1);
	REQUIRE(t.sink.CheckLastWithFCB(LinkFunction::PRI_TEST_LINK_STATES, false, true, 1, 2));
}

TEST_CASE(SUITE("ResetLinkStates"))
{
	StaticBuffer<292> buffer;
	auto writeTo = buffer.GetWriteBuffer();
	auto frame = LinkFrame::FormatResetLinkStates(writeTo, false, 1, 2, nullptr);

	LinkParserTest t;
	t.WriteData(frame);
	REQUIRE(t.log.IsLogErrorFree());
	REQUIRE(t.sink.mNumFrames ==  1);
	REQUIRE(t.sink.CheckLast(LinkFunction::PRI_RESET_LINK_STATES, false, 1, 2));
}

TEST_CASE(SUITE("RequestLinkStatus"))
{
	StaticBuffer<292> buffer;
	auto writeTo = buffer.GetWriteBuffer();
	auto frame = LinkFrame::FormatRequestLinkStatus(writeTo, true, 1, 2, nullptr);

	LinkParserTest t;
	t.WriteData(frame);
	REQUIRE(t.log.IsLogErrorFree());
	REQUIRE(t.sink.mNumFrames ==  1);
	REQUIRE(t.sink.CheckLast(LinkFunction::PRI_REQUEST_LINK_STATUS, true, 1, 2));
}

TEST_CASE(SUITE("UnconfirmedUserData"))
{
	ByteStr data(250, 0); //initializes a buffer with increasing value

	StaticBuffer<292> buffer;
	auto writeTo = buffer.GetWriteBuffer();
	auto frame = LinkFrame::FormatUnconfirmedUserData(writeTo, true, 1, 2, data, data.Size(), nullptr);

	LinkParserTest t;
	t.WriteData(frame);
	REQUIRE(t.log.IsLogErrorFree());
	REQUIRE(t.sink.mNumFrames ==  1);
	REQUIRE(t.sink.CheckLast(LinkFunction::PRI_UNCONFIRMED_USER_DATA, true, 1, 2));
	REQUIRE(t.sink.BufferEquals(data, data.Size()));
}

TEST_CASE(SUITE("ConfirmedUserData"))
{
	ByteStr data(250, 0); //initializes a buffer with increasing value

	StaticBuffer<292> buffer;
	auto writeTo = buffer.GetWriteBuffer();
	auto frame = LinkFrame::FormatConfirmedUserData(writeTo, true, true, 1, 2, data, data.Size(), nullptr);

	LinkParserTest t;
	t.WriteData(frame);
	REQUIRE(t.log.IsLogErrorFree());
	REQUIRE(t.sink.mNumFrames ==  1);
	REQUIRE(t.sink.CheckLastWithFCB(LinkFunction::PRI_CONFIRMED_USER_DATA, true, true, 1, 2));
	REQUIRE(t.sink.BufferEquals(data, data.Size()));
}

//////////////////////////////////////////
// multi packets
//////////////////////////////////////////

TEST_CASE(SUITE("TestTwoPackets"))
{
	LinkParserTest t;
	// back to back reset link
	t.WriteData("05 64 05 C0 01 00 00 04 E9 21 05 64 05 C0 01 00 00 04 E9 21");
	REQUIRE(t.log.IsLogErrorFree());
	REQUIRE(t.sink.mNumFrames ==  2);
	REQUIRE(t.sink.CheckLast(LinkFunction::PRI_RESET_LINK_STATES, true, 1, 1024));
}

//////////////////////////////////////////
// framing errors
//////////////////////////////////////////

// Test that the parser is able to resync without discarding
TEST_CASE(SUITE("Resync0564"))
{
	LinkParserTest t;
	t.WriteData("05 64 05 64 05 C0 01 00 00 04 E9 21");
	REQUIRE(t.log.NextErrorCode() ==  DLERR_CRC);
	REQUIRE(t.sink.mNumFrames ==  1);
	REQUIRE(t.sink.CheckLast(LinkFunction::PRI_RESET_LINK_STATES, true, 1, 1024));
}

//////////////////////////////////////////
// many packets
//////////////////////////////////////////

// Test that the parser is able to handle many packets successively without
// doing something stupid like overflowing it's buffer
TEST_CASE(SUITE("ManyReceives"))
{

	StaticBuffer<292> buffer;
	auto writeTo = buffer.GetWriteBuffer();
	auto frame = LinkFrame::FormatAck(writeTo, true, false, 1, 2, nullptr);

	LinkParserTest t;

	for(size_t i = 1; i < 100; ++i)
	{
		t.WriteData(frame);
		REQUIRE(t.log.IsLogErrorFree());
		REQUIRE(t.sink.mNumFrames ==  i);
		REQUIRE(t.sink.CheckLastWithDFC(LinkFunction::SEC_ACK, true, false, 1, 2));
	}
}

