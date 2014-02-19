require 'readline'
require 'orocos'
include Orocos

# Start one script on two different system and
# set this_agent and other_agent to find each other:
#
# ruby start_talking_peer.rb agent_a agent_b
# ruby start_talking_peer.rb agent_b agent_a
#

this_agent = nil
other_agent = nil

if ARGV.size != 2
    puts "usage: #{$0} <this-agent> <peer-to-communicate-with>"
else
    this_agent = ARGV[0]
    other_agent = ARGV[1]
end

Orocos.initialize
Orocos.run "fipa_services_test" do

    begin
        mts_module = TaskContext.get "mts_0"
    rescue Orocos::NotFound
        print 'Deployment not found.'
        raise
    end

    mts_module.configure
    mts_module.start

    mts_module.addReceiver(this_agent, true)

    require 'fipa-message'
    msg = FIPA::ACLMessage.new
    msg.setContent("test-content from #{this_agent} to #{other_agent} #{Time.now}")
    msg.addReceiver(FIPA::AgentId.new(other_agent))
    msg.setSender(FIPA::AgentId.new(this_agent))
    msg.setConversationID("conversation-id-#{Time.now}")

    env = FIPA::ACLEnvelope.new
    env.insert(msg, FIPARepresentation::BITEFFICIENT)

    letter_writer = mts_module.letters.writer
    letter_reader = mts_module.port(this_agent).reader

    while true
        letter_writer.write(env)

        if letter = letter_reader.read_new
            puts "#{this_agent}: received letter: #{letter.getACLMessage.getContent}"
        end
        sleep 1
    end
end
