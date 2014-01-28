require 'readline'
require 'orocos'
include Orocos

class MTS
    attr_accessor :name
    attr_accessor :orocos_task
    attr_accessor :autoconnect

    def initialize(name, orocos_task = nil)
        @name = name
        @orocos_task = orocos_task
        @autoconnect = false
    end

    def getAllOthers(otherMTS)
        others = otherMTS.dup
        others.delete_if {|m| m.name == name}
        others
    end

    def connectToMTS(otherMTS)
        if not orocos_task.addReceiver(otherMTS.name, false)
            raise RuntimeError, "Connection failed"
        end

        output_port = orocos_task.port(otherMTS.name)
        input_port = otherMTS.orocos_task.port("letters")

        output_sd_port = orocos_task.local_service_directory
        input_sd_port = otherMTS.orocos_task.port("service_directories")

        output_port.connect_to input_port, :type => :buffer, :size => 100
        output_sd_port.connect_to input_sd_port, :type => :buffer, :size => 100
    end

    def start
        orocos_task.autoconnect = @autoconnect
        orocos_task.configure
        orocos_task.start
    end
end

class Agent
    attr_accessor :name

    attr_reader :input_reader
    attr_reader :output_writer

    def initialize(name)
        @name = name
    end

    def attach_to_mts(mts)
        # add local receiver
        if not mts.orocos_task.addReceiver(name, true)
            raise RuntimeError, "Could not add receiver"
        end
        @input_reader = mts.orocos_task.port(name).reader
        @output_writer = mts.orocos_task.port("letters").writer
    end

    def send_letter(letter)
        puts "send: #{Time.now}"
        @output_writer.write(letter)
    end
    def read_next_letter
        while letter = @input_reader.read_new
            puts "receive: #{Time.now}"
            puts "Agent: #{name} --> content: #{letter.getACLMessage.getContent}"
        end
        letter
    end

end

all_mts = Array.new
(0..2).each do |i|
    mts = MTS.new("mts_#{i}")
    all_mts << mts
end


Orocos.initialize
Orocos.run "fipa_services_test", :cmdline_args => { "sd-domain" => "_test_tt._tcp" }, :wait => 20, :valgrind => false do 
    # Resolve orocos tasks
    begin
        all_mts.each do |mts|
            mts.orocos_task = TaskContext.get mts.name
            mts.autoconnect = true
        end
    rescue Orocos::NotFound
        print 'Deployment not found.'
        raise
    end
  
    all_mts.each do |mts|
        if not mts.autoconnect
            others = mts.getAllOthers(all_mts)
            others.each do |otherMTS|
                mts.connectToMTS otherMTS
            end
        end
    end

    all_mts.each do |m| 
        m.start
    end

    puts "All mts connected and started"
    Readline::readline("Press ENTER to attach agents")

    agents = Array.new
    (0..2).each do |i|
        agent = Agent.new("agent_#{i}")
        agents << agent
        agent.attach_to_mts(all_mts[i])
    end

    Readline::readline("Press ENTER after attaching agents")

    require 'fipa-message'
    msg = FIPA::ACLMessage.new
    msg.setContent("test-content")
    msg.addReceiver(FIPA::AgentId.new("agent_1"))
    msg.addReceiver(FIPA::AgentId.new("agent_2"))
    msg.setSender(FIPA::AgentId.new("agent_0"))
    msg.setConversationID("bla-one")
    env = FIPA::ACLEnvelope.new
    env.insert(msg, FIPARepresentation::BITEFFICIENT)

    # Send mesg
    agents[0].send_letter(env)

    # Read msg
    (0..5).each do |i|
        agents.each do |a|
            a.read_next_letter
        end
        sleep 0.1
    end
        
    Readline::readline("Press ENTER to proceed")
end 
