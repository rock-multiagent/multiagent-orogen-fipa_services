require 'readline'
require 'orocos'
include Orocos

Orocos.initialize

class MTS
    attr_accessor :name
    attr_accessor :orocos_task

    def initialize(name, orocos_task = nil)
        @name = name
        @orocos_task = orocos_task
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

        #puts "CONNECTION #{name} to #{otherMTS.name}"
        #puts "o: #{output_port} i: #{input_port}"

        output_port.connect_to input_port, :type => :buffer, :size => 100
    end

    def start
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
(0..4).each do |i|
    mts = MTS.new("mts_#{i}")
    all_mts << mts
end

Orocos.run "fipa_services_test", :wait => 1, :valgrind => false do 
    Readline::readline("Press ENTER to proceed")
    # Resolve orocos tasks
    begin
        all_mts.each do |mts|
            mts.orocos_task = TaskContext.get mts.name
        end
    rescue Orocos::NotFound
        print 'Deployment not found.'
        raise
    end
    
    all_mts.each do |mts|
        others = mts.getAllOthers(all_mts)
        others.each do |otherMTS|
            mts.connectToMTS otherMTS
        end
    end

    all_mts.each { |m| m.start }
#    all_mts.each { |m| m.orocos_task.publishConnectionStatus }

    puts "All mts connected and started"
    Readline::readline("Press ENTER to attach agents")

    agents = Array.new
    (0..4).each do |i|
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
        sleep 1
    end
        
    Readline::readline("Press ENTER to proceed")
end 
