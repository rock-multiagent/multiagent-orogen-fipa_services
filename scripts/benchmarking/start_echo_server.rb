require 'readline'
require 'orocos'
require_relative 'fipa_benchmark'
require 'optparse'
require 'securerandom'

uuid = SecureRandom.uuid.gsub("-","")

include Orocos

o_ntp_sync = false
o_this_agent = "echo-#{uuid}"
o_other_agents = nil

allowed_transports = [ "UDT", "TCP"]
o_transport = "UDT"

options = OptionParser.new do |opts|
    opts.banner = "usage: #{$0}"
    opts.on("-o","--agent NAME", "Name of this (echoing) agent") do |name|
        o_this_agent = "#{name}-#{uuid}"
    end

    opts.on("-s","--ntp-sync","Activate if ntp shall be synced at startup") do
        o_ntp_sync = true
    end

    opts.on("-t","--transport TYPE", "Select transport type: either TCP or UDT") do |transport|
        if allowed_transports.include?(transport)
            o_transport = transport
        else
            puts "Transport '#{transport}' is unknown -- select one of #{allowed_transports.join(',')}"
            exit 1
        end
    end

    opts.on("-h","--help") do
        puts opts
        exit 0
    end
end

unhandled_arguments = options.parse(ARGV)

if o_ntp_sync
    system("sudo bash #{File.join(__dir__,'prepare_ntp.sh')}")
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

    mts_module.transports = [ o_transport ]
    mts_module.configure
    mts_module.start
    mts_module.addReceiver(o_this_agent, true)

    Orocos.log_all_ports

    echo_task.agent_name = o_this_agent
    echo_task.configure
    echo_task.start

    mts_module.port(o_this_agent).connect_to echo_task.letters
    echo_task.handled_letters.connect_to mts_module.letters

    Orocos.watch(echo_task, mts_module)

end
