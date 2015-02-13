require 'orocos'
require 'readline'
require 'fipa-message'
include Orocos
Orocos.initialize

# This test the setup up two mts "blue" and "red"
#
# [ MTS: blue ]-blue_client
# [ MTS: red  ]-red_client
#
# 1. send messsage from red_client to blue_client --> should succeed
# 2. send message from red_client to blue_unknown_client --> should fail
Orocos.run "fipa_services::MessageTransportTask" => ["blue-mts", "red-mts"] , :valgrind => false do

    blue = TaskContext.get 'blue-mts'
    blue.transports = ["tcp"]
    blue.configure
    blue.start
    blue.addReceiver("blue_client", true)

    red = TaskContext.get 'red-mts'
    red.transports = ["tcp"]
    red.configure
    red.start
    red.addReceiver("red_client", true)

    sleep 2

    msg = FIPA::ACLMessage.new
    msg.setContent("test-content")
    msg.addReceiver(FIPA::AgentId.new("blue_unknown_client"))
    msg.addReceiver(FIPA::AgentId.new("blue_client"))
    msg.setSender(FIPA::AgentId.new("red_client"))

    env = FIPA::ACLEnvelope.new
    env.insert(msg, FIPARepresentation::BITEFFICIENT)

    puts "first to: #{env.getTo[0].getName}"
    puts "first from: #{env.getFrom.getName}"

    data = FIPA::Utils.serialize_envelope(env)
    puts "Data #{data.size}"
    testenv = FIPA::Utils.deserialize_envelope(data)

    puts "TO: #{testenv.getTo[0].getName}"
    puts "From: #{testenv.getFrom.getName}"
    puts "Content: #{testenv.getACLMessage.getContent}"

    postman = red.letters.writer
    postman.write(env)

    blue_client_reader = blue.blue_client.reader
    red_client_reader = red.red_client.reader

    while true 
        if envelope = blue_client_reader.read_new
            puts "Blue client received data"
            puts "DeliveryPath:"
            envelope.getDeliveryPath.each do |agent_id|
                puts "#{agent_id.getName}"
            end
            puts "Content: #{envelope.getACLMessage.getContent}"
        end

        if envelope = red_client_reader.read_new
            puts "Red client received data"
            puts "DeliveryPath:"
            envelope.getDeliveryPath.each do |agent_id|
                puts "#{agent_id.getName}"
            end
            puts "Content: #{envelope.getACLMessage.getContent}"
        end

        puts "Waiting"
        sleep 1
        Readline::readline("Press ENTER to proceed")
    end
    Readline::readline("Press ENTER to proceed")
end
