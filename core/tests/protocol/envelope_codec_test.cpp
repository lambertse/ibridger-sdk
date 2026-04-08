#include "ibridger/protocol/envelope_codec.h"

#include <gtest/gtest.h>

#include "ibridger/common/error.h"
#include "ibridger/protocol/framing.h"
#include "test_transport_pair.h"

using ibridger::protocol::EnvelopeCodec;
using ibridger::protocol::FramedConnection;

namespace {

/// Builds a fully-populated Envelope with every field set.
ibridger::Envelope make_full_envelope() {
  ibridger::Envelope env;
  env.set_type(ibridger::REQUEST);
  env.set_request_id(123456789ULL);
  env.set_service_name("com.example.EchoService");
  env.set_method_name("Echo");
  env.set_payload("binary\x00payload\xFF", 15);
  env.set_status(ibridger::OK);
  env.set_error_message("");
  (*env.mutable_metadata())["trace-id"] = "abc-123";
  (*env.mutable_metadata())["user-agent"] = "ibridger-test/1.0";
  (*env.mutable_metadata())["locale"] = "en-US";
  return env;
}

}  // namespace

// ─── Full roundtrip (all field types) ────────────────────────────────────────

TEST(EnvelopeCodec, FullRoundtrip) {
  auto [sender, receiver] = ibridger::test::make_codec_pair();

  ibridger::Envelope sent = make_full_envelope();
  ASSERT_FALSE(sender.send(sent));

  auto [received, err] = receiver.recv();
  ASSERT_FALSE(err) << err.message();

  EXPECT_EQ(received.type(), ibridger::REQUEST);
  EXPECT_EQ(received.request_id(), 123456789ULL);
  EXPECT_EQ(received.service_name(), "com.example.EchoService");
  EXPECT_EQ(received.method_name(), "Echo");
  EXPECT_EQ(received.payload(), sent.payload());
  EXPECT_EQ(received.status(), ibridger::OK);
}

// ─── Request / response with correlated request_id ───────────────────────────

TEST(EnvelopeCodec, RequestResponseCorrelation) {
  auto [client, server] = ibridger::test::make_codec_pair();

  // Client sends a REQUEST.
  ibridger::Envelope request;
  request.set_type(ibridger::REQUEST);
  request.set_request_id(42ULL);
  request.set_service_name("PingService");
  request.set_method_name("Ping");
  request.set_payload("ping-payload");
  ASSERT_FALSE(client.send(request));

  // Server receives it.
  auto [received_req, req_err] = server.recv();
  ASSERT_FALSE(req_err) << req_err.message();
  EXPECT_EQ(received_req.request_id(), 42ULL);
  EXPECT_EQ(received_req.type(), ibridger::REQUEST);

  // Server sends a RESPONSE with the same request_id.
  ibridger::Envelope response;
  response.set_type(ibridger::RESPONSE);
  response.set_request_id(received_req.request_id());  // correlation
  response.set_status(ibridger::OK);
  response.set_payload("pong-payload");
  ASSERT_FALSE(server.send(response));

  // Client receives the response.
  auto [received_resp, resp_err] = client.recv();
  ASSERT_FALSE(resp_err) << resp_err.message();
  EXPECT_EQ(received_resp.type(), ibridger::RESPONSE);
  EXPECT_EQ(received_resp.request_id(), 42ULL);  // id must be preserved
  EXPECT_EQ(received_resp.status(), ibridger::OK);
  EXPECT_EQ(received_resp.payload(), "pong-payload");
}

// ─── Corrupted payload returns parse error, doesn't crash ────────────────────

TEST(EnvelopeCodec, CorruptedPayloadReturnsParseError) {
  // Use the framing layer to inject invalid protobuf bytes — works on all
  // platforms because we go through the normal FramedConnection send path.
  auto [injector, framed_recv] = ibridger::test::make_framed_pair();

  auto shared_recv = std::make_shared<FramedConnection>(std::move(framed_recv));
  EnvelopeCodec receiver(shared_recv);

  const std::string garbage = "\xFF\xFE\xFD not a valid protobuf \x00\x01\x02";
  ASSERT_FALSE(injector.send_frame(garbage));

  auto [env, err] = receiver.recv();
  EXPECT_EQ(err, ibridger::common::make_error_code(
                     ibridger::common::Error::serialization_error))
      << "Expected serialization_error for corrupted payload, got: "
      << err.message();
  // The returned Envelope must be empty/default — not partially populated.
  EXPECT_EQ(env.request_id(), 0ULL);
  EXPECT_EQ(env.service_name(), "");
}

// ─── Metadata map preservation ───────────────────────────────────────────────

TEST(EnvelopeCodec, MetadataMapPreservation) {
  auto [sender, receiver] = ibridger::test::make_codec_pair();

  ibridger::Envelope env;
  env.set_type(ibridger::REQUEST);
  env.set_request_id(1ULL);
  (*env.mutable_metadata())["key-a"] = "value-a";
  (*env.mutable_metadata())["key-b"] = "value-b";
  (*env.mutable_metadata())["key-c"] = "value-c";
  ASSERT_FALSE(sender.send(env));

  auto [received, err] = receiver.recv();
  ASSERT_FALSE(err) << err.message();

  ASSERT_EQ(received.metadata().size(), 3u);
  EXPECT_EQ(received.metadata().at("key-a"), "value-a");
  EXPECT_EQ(received.metadata().at("key-b"), "value-b");
  EXPECT_EQ(received.metadata().at("key-c"), "value-c");
}

// ─── Error envelope (status + error_message) ─────────────────────────────────

TEST(EnvelopeCodec, ErrorEnvelopeRoundtrip) {
  auto [sender, receiver] = ibridger::test::make_codec_pair();

  ibridger::Envelope env;
  env.set_type(ibridger::ERROR);
  env.set_request_id(7ULL);
  env.set_status(ibridger::NOT_FOUND);
  env.set_error_message("service 'ghost' not registered");
  ASSERT_FALSE(sender.send(env));

  auto [received, err] = receiver.recv();
  ASSERT_FALSE(err) << err.message();
  EXPECT_EQ(received.type(), ibridger::ERROR);
  EXPECT_EQ(received.status(), ibridger::NOT_FOUND);
  EXPECT_EQ(received.error_message(), "service 'ghost' not registered");
  EXPECT_EQ(received.request_id(), 7ULL);
}
