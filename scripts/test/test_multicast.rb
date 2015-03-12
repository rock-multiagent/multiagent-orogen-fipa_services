require 'orocos'
require 'readline'
require 'fipa-message'
include Orocos
Orocos.initialize

# This test the setup up two mts "blue" and "red"
#
# [ MTS: blue   ]-blue_client
# [ MTS: red    ]-red_client
# [ MTS: yellow ]-yellow_client
#
# 1. send message from red_client to blue_client and yellow_client via broadcast --> should succeed
Orocos.run "fipa_services::MessageTransportTask" => ["blue-mts", "red-mts","yellow-mts"] , :valgrind => false do

    blue = TaskContext.get 'blue-mts'
    blue.configure
    blue.start
    blue.addReceiver("blue_client", true)

    red = TaskContext.get 'red-mts'
    red.configure
    red.start
    red.addReceiver("red_client", true)

    yellow = TaskContext.get 'yellow-mts'
    yellow.configure
    yellow.start
    yellow.addReceiver("yellow_client", true)
    yellow.addReceiver("other", true)

    sleep 2

    msg = FIPA::ACLMessage.new
    msg.setContent("test-content")
    msg.setSender(FIPA::AgentId.new("blue_client"))
    msg.addReceiver(FIPA::AgentId.new(".*_client"))

    env = FIPA::ACLEnvelope.new
    env.insert(msg, FIPARepresentation::BITEFFICIENT)

    postman = blue.letters.writer
    postman.write(env)

    blue_client_reader = blue.blue_client.reader
    red_client_reader = red.red_client.reader
    yellow_client_reader = yellow.yellow_client.reader
    other_reader = yellow.other.reader

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

        if envelope = yellow_client_reader.read_new
            puts "Yellow client received data"
            puts "DeliveryPath:"
            envelope.getDeliveryPath.each do |agent_id|
                puts "#{agent_id.getName}"
            end
            puts "Content: #{envelope.getACLMessage.getContent}"
        end

        if envelope = other_reader.read_new
            puts "Yellow's other (client) received data"
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
