require 'readline'
require 'orocos'
require './fipa_benchmark.rb'

include Orocos

this_agent = nil
other_agents = nil

if ARGV.size != 1
    puts "usage: #{$0} <this-agent>"
    exit 0
else
    this_agent = ARGV[0]
end

Orocos.initialize
Orocos.run "fipa_services::MessageTransportTask" => "mts_echo",
    "fipa_services::EchoTask" => 'echo' do

    begin
        mts_module = TaskContext.get "mts_echo"
        echo_task = TaskContext.get 'echo'
    rescue Orocos::NotFound
        print 'Deployment not found.'
        raise
    end

    mts_module.configure
    mts_module.start
    mts_module.addReceiver(this_agent, true)

    Orocos.log_all_ports

    echo_task.agent_name = this_agent
    echo_task.configure
    echo_task.start

    mts_module.port(this_agent).connect_to echo_task.letters
    echo_task.handled_letters.connect_to mts_module.letters

    Orocos.watch(echo_task, mts_module)

end
