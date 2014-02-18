require 'readline'
require 'orocos'
include Orocos

# After startup check for two existing services listed in 
# _fipa_service_directory._udp domain using
# avahi-discover
Orocos.initialize
Orocos.run "fipa_services_test" do

    begin
        mts_0 = TaskContext.get "mts_0"
        mts_1 = TaskContext.get "mts_1"
    rescue Orocos::NotFound
        print 'Deployment not found.'
        raise
    end

    mts_0.configure
    mts_0.start

    mts_1.configure
    mts_1.start

    mts_0.addReceiver("local_client-0", true)
    mts_1.addReceiver("local_client-1", true)

    Readline::readline("Press ENTER to exit")
end 
