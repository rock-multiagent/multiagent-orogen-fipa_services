require 'readline'
require 'orocos'
require_relative 'fipa_benchmark'
require 'optparse'
require 'securerandom'

uuid = SecureRandom.uuid.gsub("-","")

include Orocos

o_ntp_sync = false
o_this_agent = "origin-#{uuid}"
o_other_agents = nil

allowed_transports = [ "UDT", "TCP"]
o_transport = "UDT"

options = OptionParser.new do |opts|
    opts.banner = "usage: #{$0}"
    opts.on("-o","--agent NAME", "Name of this (sending) agent") do |name|
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
Orocos.run "fipa_services::MessageTransportTask" => "mts_0" do
    begin
        mts_module = TaskContext.get "mts_0"
    rescue Orocos::NotFound
        print 'Deployment not found.'
        raise
    end

    mts_module.transports = [ o_transport ]
    mts_module.configure
    mts_module.start

    mts_module.addReceiver(o_this_agent, true)
    benchmark = FIPA::Benchmark.new(mts_module, o_this_agent, "echo-.*")

    Orocos.log_all_ports

    letter_writer = mts_module.letters.writer
    letter_reader = mts_module.port(o_this_agent).reader

    benchmark.send
    Orocos.watch(mts_module)
end
