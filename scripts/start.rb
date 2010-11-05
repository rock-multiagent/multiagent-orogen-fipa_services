require 'FipaMessage'

require 'orocos'
include Orocos

Orocos.initialize

# as listed in the orogen definition
#Orocos::Process.spawn 'root_module', 'valgrind'=> false, "wait" => 100  do |process|
Orocos.run 'root_module', 'root_module_', "wait" => 100 do |p1,p2|
#Orocos.run 'root_module', "wait" => 100 do |p1|
  #supervision code
  #as given in the corresponding deployment
begin
    root_module = p1.task 'root_module'
    root_module_ = p2.task 'root_module_'
rescue Orocos::NotFound
    print 'Deployment not found.'
    raise
end
    #root_module = TaskContext.get 'root_module'
    root_module.module_name = 'A_ROOT_1'
    root_module.avahi_type = '_rimres._tcp'
    root_module.avahi_port = 12000
    root_module.avahi_ttl = 0
    root_module.configure
    root_module.start
        
    root_module_.module_name = 'A_ROOT_2'
    root_module_.avahi_type = '_rimres._tcp'
    root_module_.avahi_port = 12000
    root_module_.avahi_ttl = 0
    root_module_.configure
    root_module_.start

    sleep 4

    # Generate and send fipa message.
	performative = "inform"
    msg = FipaMessage.new
    msg.setPerformative(performative)
    msg.setContent("Test message")
    in_port = root_module.inputPortMTS
    writer = in_port.writer

    puts 'Schreibe '
    writer.write(msg)

    while true
         #puts 'Schreibe '
         #writer.write(msg)
        sleep 1
    end
end 
