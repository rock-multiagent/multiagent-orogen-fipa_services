require 'readline'
require 'orocos'
include Orocos

Orocos.initialize
Orocos.run "fipa_services_test" do

    begin
        mts_module = TaskContext.get "mts_0"
    rescue Orocos::NotFound
        print 'Deployment not found.'
        raise
    end

    mts_module.nic = 'eth0'
    mts_module.configure
    mts_module.start

    mts_module.addReceiver("local_client", true)
    while true
        sleep 1
    end
end 
