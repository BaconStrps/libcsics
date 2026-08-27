#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <csics/csics.hpp>

#include "csics/lvc/dis/PDUs.hpp"
#include "csics/lvc/dis/serde.hpp"
#include "csics/serialization/Common.hpp"

csics::lvc::dis::PDUHeader create_sample_pdu_header() {
    using namespace csics::lvc;
    dis::PDUHeader header;
    header.protocol_version = dis::ProtocolVersion::IEEE_1278_2012;
    header.exercise_id = 1;
    header.protocol_family = 3;
    header.timestamp = dis::DISTimestamp(123456789);
    header.status = 0;

    return header;
}

csics::lvc::dis::EntityStatePDU create_sample_entity_state_pdu() {
    using namespace csics::lvc;
    dis::EntityStatePDU csics_pdu{};
    csics_pdu.header = create_sample_pdu_header();
    csics_pdu.header.pdu_type = dis::PDUType::EntityState;
    csics_pdu.entity_id = dis::EntityID(1, 2, 3);
    csics_pdu.force_id = 0;
    csics_pdu.alternative_entity_type = dis::EntityType(1, 2, 3, 4, 5, 6, 7);
    csics_pdu.entity_type = dis::EntityType(7, 8, 9, 10, 11, 12, 13);
    csics_pdu.entity_linear_velocity = dis::Vector(1.0f, 2.0f, 3.0f);
    csics_pdu.entity_location = dis::WorldCoordinates(4.0f, 5.0f, 6.0f);
    csics_pdu.entity_orientation = dis::EulerAngles(0.1f, 0.2f, 0.3f);
    csics_pdu.entity_appearance = 0x12345678;
    csics_pdu.dr_parameters.algorithm = 1;
    csics_pdu.dr_parameters.params_type = 1;  // fixed
    csics_pdu.dr_parameters.fixed.local_angles =
        dis::EulerAngles(0.01f, 0.02f, 0.03f);
    csics_pdu.dr_parameters.linear_acceleration = dis::Vector(0.1f, 0.2f, 0.3f);
    csics_pdu.dr_parameters.angular_velocity = dis::Vector(0.01f, 0.02f, 0.03f);

    return csics_pdu;
}

csics::lvc::dis::ElectromagneticEmissionPDU create_sample_emissions_pdu() {
    using namespace csics::lvc;
    dis::ElectromagneticEmissionPDU csics_pdu{};
    csics_pdu.header = create_sample_pdu_header();
    csics_pdu.header.pdu_type = dis::PDUType::ElectromagneticEmission;
    csics_pdu.emitter_id = dis::EntityID(1, 2, 3);
    csics_pdu.event_id = dis::EventID(1, 2, 6);
    csics_pdu.state_update_indicator = 0;

    dis::EmitterSystem system{};
    system.emitter_number = 0;
    system.emitter_name = 0x1234;
    system.function = 0;
    system.emitter_location = dis::EntityCoordinates(1.0f, 2.0f, 3.0f);
    std::cerr << "Pushing back beam..." << std::endl;
    system.beams.push_back(dis::Beam());
    system.beams[0].beam_number = 0;
    system.beams[0].beam_function = 0;
    system.beams[0].beam_parameter_index = 0;
    system.beams[0].fundamental_parameters.center_frequency = 1000.0f;
    system.beams[0].fundamental_parameters.frequency_range = 2000.0f;
    system.beams[0].fundamental_parameters.pulse_repetition_frequency = 300.0f;
    system.beams[0].fundamental_parameters.pulse_width = 0.01f;
    system.beams[0].fundamental_parameters.effective_radiated_power = 0.5f;
    system.beams[0].jamming_technique = {0x01, 0x02, 0x03, 0x04};
    system.beams[0].number_of_targets = 1;
    std::cerr << "Pushing back track jam..." << std::endl;
    system.beams[0].track_jams = {dis::TrackJam()};
    std::cerr << "Pushing back emitter system" << std::endl;
    csics_pdu.emitter_systems.push_back(system);
    static_assert(
        !std::is_trivially_copyable_v<csics::Buffer<dis::EmitterSystem>>);
    static_assert(!std::is_trivially_copyable_v<csics::Buffer<dis::Beam>>);
    static_assert(std::is_trivially_copyable_v<dis::TrackJam>);
    static_assert(!std::is_trivially_copyable_v<dis::Beam>);
    static_assert(!std::is_trivially_copyable_v<dis::EmitterSystem>);
    static_assert(!std::is_trivially_copyable_v<
                  std::optional<csics::Buffer<dis::TrackJam>>>);

    return csics_pdu;
}

TEST(CSICSLVCTests, DIS7Serialization) {
    using namespace csics::lvc;
    std::cerr << "Creating sample PDUs..." << std::endl;

    // Create sample PDUs
    dis::EntityStatePDU entity_state_pdu = create_sample_entity_state_pdu();
    std::cerr << "Created EntityStatePDU" << std::endl;
    dis::ElectromagneticEmissionPDU emissions_pdu =
        create_sample_emissions_pdu();
    std::cerr << "Created ElectromagneticEmissionPDU" << std::endl;

    // Serialize using CSICS
    csics::Buffer<char> entity_state_buffer(1024);
    csics::Buffer<char> emissions_buffer(1024);

    csics::serialization::DirectSerializer s;
    std::cerr << "Serializing EntityStatePDU with CSICS..." << std::endl;
    std::cerr.flush();
    auto res = csics::serialization::serialize(s, entity_state_buffer,
                                               entity_state_pdu);

    ASSERT_EQ(res.status, csics::serialization::SerializationStatus::Ok)
        << "Serialization failed with status: " << static_cast<int>(res.status);
    ASSERT_FALSE(res.written_view.empty())
        << "Serialization did not write any bytes";

    ASSERT_EQ(res.written_view.size(), dis::pdu_size_calc(entity_state_pdu))
        << "Serialized size does not match expected size";

    std::cerr << "Deserializing EntityStatePDU with DIS7..." << std::endl;
    dis::EntityStatePDU dis_entity_state_pdu;
    csics::serialization::DirectDeserializer d(entity_state_buffer);
    auto pdu = csics::serialization::deserialize(d, dis_entity_state_pdu);
    ASSERT_TRUE(pdu.has_value());
    ASSERT_EQ(pdu->header.protocol_version,
              dis::ProtocolVersion::IEEE_1278_2012);
    ASSERT_EQ(pdu->entity_id, dis::EntityID(1, 2, 3));
    ASSERT_EQ(pdu->entity_location, dis::WorldCoordinates(4.0f, 5.0f, 6.0f));
    ASSERT_EQ(pdu->entity_orientation, dis::EulerAngles(0.1f, 0.2f, 0.3f));
    ASSERT_EQ(pdu->entity_appearance, 0x12345678);
    ASSERT_EQ(pdu->dr_parameters.fixed.local_angles,
              dis::EulerAngles(0.01f, 0.02f, 0.03f));

    std::cerr << "Serializing ElectromagneticEmissionPDU with CSICS..."
              << std::endl;
    res = csics::serialization::serialize(s, emissions_buffer, emissions_pdu);
    ASSERT_EQ(res.status, csics::serialization::SerializationStatus::Ok)
        << "Serialization failed with status: " << static_cast<int>(res.status);
    ASSERT_FALSE(res.written_view.empty())
        << "Serialization did not write any bytes";
    ASSERT_EQ(res.written_view.size(), dis::pdu_size_calc(emissions_pdu))
        << "Serialized size does not match expected size";

    std::cerr << "Deserializing ElectromagneticEmissionPDU with DIS7..."
              << std::endl;
    dis::ElectromagneticEmissionPDU dis_emissions_pdu;
    csics::serialization::DirectDeserializer d2(emissions_buffer);
    auto pdu2 = csics::serialization::deserialize(d2, dis_emissions_pdu);
    ASSERT_TRUE(pdu2.has_value());
    ASSERT_EQ(pdu2->header.protocol_version,
              dis::ProtocolVersion::IEEE_1278_2012);
    ASSERT_EQ(pdu2->emitter_id, dis::EntityID(1, 2, 3));

    ASSERT_EQ(pdu2->emitter_systems.size(), 1);
    const auto& emitter_system = pdu2->emitter_systems[0];
    ASSERT_EQ(emitter_system.emitter_name, 0x1234);
    ASSERT_EQ(emitter_system.emitter_location,
              dis::EntityCoordinates(1.0f, 2.0f, 3.0f));
    ASSERT_EQ(emitter_system.beams.size(), 1);
    const auto& beam = emitter_system.beams[0];
    ASSERT_EQ(beam.fundamental_parameters.center_frequency, 1000.0f);
    ASSERT_EQ(beam.fundamental_parameters.frequency_range, 2000.0f);
    ASSERT_EQ(beam.fundamental_parameters.pulse_repetition_frequency, 300.0f);
}
