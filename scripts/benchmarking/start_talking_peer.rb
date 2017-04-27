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
Orocos.run "fipa_services::MessageTransportTask" => "mts_0" do
    begin
        mts_module = TaskContext.get "mts_0"
    rescue Orocos::NotFound
        print 'Deployment not found.'
        raise
    end

    mts_module.configure
    mts_module.start

    mts_module.addReceiver(this_agent, true)
    benchmark = FIPA::Benchmark.new(mts_module, this_agent, "echo-.*")

    Orocos.log_all_ports

    letter_writer = mts_module.letters.writer
    letter_reader = mts_module.port(this_agent).reader

    benchmark.send
    Orocos.watch(mts_module)
end
