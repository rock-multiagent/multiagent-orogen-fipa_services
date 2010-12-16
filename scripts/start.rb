require 'fipa-message'

require 'orocos'
include Orocos

Orocos.initialize

Orocos.run 'A_ROOT_1', "wait" => 100 do |p1|

begin
    root_module = p1.task 'A_ROOT_1'
rescue Orocos::NotFound
    print 'Deployment not found.'
    raise
end
    # deprecated: has no effect, use dedicate deployments instead
    root_module.module_name = 'A_ROOT_1'

    root_module.avahi_type = '_rimres._tcp'
    root_module.avahi_port = 12000
    root_module.avahi_ttl = 0
    root_module.configure

    # Generate and send fipa message.
    performative = "inform"
    msg = FipaMessage.new
    msg.setPerformative(performative)
    agent = FipaAgentId.new("A_MTA_1")
    msg.setContent("Test message")
    in_port = root_module.inputPortMTS
    writer = in_port.writer

    root_module.start

    while true
         #puts 'Schreibe '
         #writer.write(msg)
        sleep 1
    end
end 
