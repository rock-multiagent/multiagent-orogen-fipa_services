require 'readline'
require 'orocos'
include Orocos

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

    mts_0.local_service_directory.connect_to mts_1.service_directories
    mts_1.local_service_directory.connect_to mts_0.service_directories

    mts_0.addReceiver("local_client_of_mts0", true)
    mts_1.addReceiver("local_client_of_mts1", true)

    while true
        sleep 1
    end
end 
