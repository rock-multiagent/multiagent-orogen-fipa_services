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

if ARGV.size != 1
    puts "usage: #{$0} <this-agent>"
    exit 0
else
    this_agent = ARGV[0]
end

Orocos.initialize
Orocos.run 'fipa_services::MessageTransportTask' => 'mts_module_receiver' do
    ## Get the task context##
    mts_module = Orocos.name_service.get 'mts_module_receiver'

    mts_module.configure
    mts_module.start

    mts_module.addReceiver(this_agent, true)

    letter_writer = mts_module.letters.writer
    letter_reader = mts_module.port(this_agent).reader

    while true
        if letter = letter_reader.read_new
            puts "#{this_agent}: received letter: #{letter.getACLMessage.getContent}"
        end
        sleep 0.01
    end
end
