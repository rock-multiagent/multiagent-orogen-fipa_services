require 'readline'
require 'orocos'
include Orocos

Orocos.initialize

# Simple startup of a message transport
# You can check whether the 'local_client' is part of the distributed service
# directory
# using one of the following commands:
#    - service_discovery-browse _fipa_service_directory._udp
#    - avahi-browse _fipa_service_directory._udp
#
Orocos.run "fipa_services::MessageTransportTask" => "mts" do

    begin
        mts_module = TaskContext.get "mts"
    rescue Orocos::NotFound
        print 'Deployment not found.'
        raise
    end

    mts_module.configure
    mts_module.start

    mts_module.addReceiver("local_client", true)

    Readline::readline("Press ENTER to proceed")
end 
