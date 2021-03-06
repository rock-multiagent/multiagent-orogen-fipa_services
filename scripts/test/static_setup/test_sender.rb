require 'readline'
require 'orocos'
include Orocos

# Start one script on two different system and
# set this_agent and other_agent to find each other:
#
# ruby test_sender.rb agent_a agent_b
# ruby test_receiver.rb agent_b
#

this_agent = nil
other_agent = nil

if ARGV.size != 2
    puts "usage: #{$0} <this-agent> <peer-to-communicate-with>"
    exit 0
else
    this_agent = ARGV[0]
    other_agent = ARGV[1]
end

Orocos.initialize
Orocos.run 'fipa_services::MessageTransportTask' => 'mts_module_sender' do
    ## Get the task context##
    mts_module = Orocos.name_service.get 'mts_module_sender'
    
    Orocos.apply_conf_file(mts_module, 'sender.yml', ['default'])
    mts_module.configure
    mts_module.start

    mts_module.addReceiver(this_agent, true)

    letter_writer = mts_module.letters.writer
    letter_reader = mts_module.port(this_agent).reader

    require 'fipa-message'
    msg = FIPA::ACLMessage.new
    msg.addReceiver(FIPA::AgentId.new(other_agent))
    msg.setSender(FIPA::AgentId.new(this_agent))

    while true
        if letter = letter_reader.read
            puts "Agent: #{this_agent} --> content: #{letter.getACLMessage.getContent}"
        end

        Readline::readline("Press ENTER to send a message")
        
        msg.setContent("test-content from #{this_agent} to #{other_agent} #{Time.now}")
        msg.setConversationID("conversation-id-#{Time.now}")
        
        env = FIPA::ACLEnvelope.new
        env.insert(msg, FIPARepresentation::BITEFFICIENT)
        
        letter_writer.write(env)
    end
end
