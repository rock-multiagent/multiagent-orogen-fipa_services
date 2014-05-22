require 'orocos'

include Orocos
Orocos.initialize

# Test producing a message delivery failure message
Orocos.run "fipa_services_test"  do
    puts "Test MTS failure"
    
    # Start two mts for the communication
    begin
        mts_module0 = TaskContext.get "mts_0"
        mts_module1 = TaskContext.get "mts_1"
    rescue Orocos::NotFound
        print 'Deployment not found.'
        raise
    end
    mts_module0.configure
    mts_module0.start
    mts_module1.configure
    mts_module1.start
    
    agent0 = "agent0"
    agent1 = "agent1"
    
    mts_module0.addReceiver(agent0, true)
    mts_module1.addReceiver(agent1, true)
    
    agent0_letter_writer = mts_module0.letters.writer
    agent1_letter_reader = mts_module1.port(agent1).reader
    
    
    while true
        Readline::readline("Press ENTER to send a msg.")
        
        # Send message
        require 'fipa-message'
        msg = FIPA::ACLMessage.new
        msg.setContent("test-content from #{agent0} to #{agent1} #{Time.now}")
        msg.addReceiver(FIPA::AgentId.new(agent1))
        
        msg.setSender(FIPA::AgentId.new(agent0))
        msg.setConversationID("agent0_cid")

        env = FIPA::ACLEnvelope.new
        env.insert(msg, FIPARepresentation::BITEFFICIENT)
        
        agent0_letter_writer.write(env)
        
        # receive message(s)
        sleep 0.5
        while letter = agent1_letter_reader.read_new
            puts "#{agent1}: received letter: #{letter.getACLMessage.getContent}"
            sleep 0.5
        end
        
        # stop and start mts_1
        puts "restarting receiving mts"
        mts_module1.stop
        sleep 2
        mts_module1.start
    end
end