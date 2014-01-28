require 'readline'
require 'orocos'
include Orocos

Orocos.initialize
Orocos.run "fipa_services::MessageTransportTask" => ['mts_5','mts_6'] , :cmdline_args => { 'sd-domain' => '_test_tt._tcp' }, :wait => 5 do

    begin
        mts_0 = TaskContext.get "mts_5"
        mts_1 = TaskContext.get "mts_6"

        mts_0.autoconnect = true
        mts_1.autoconnect = true
    rescue Orocos::NotFound
        print 'Deployment not found.'
        raise
    end

    mts_0.configure
    mts_0.start

    mts_1.configure
    mts_1.start

#    mts_0.local_service_directory.connect_to mts_1.service_directories
#    mts_1.local_service_directory.connect_to mts_0.service_directories

    #mts_0.addReceiver("local_client_of_mts0", true)
    #mts_1.addReceiver("local_client_of_mts1", true)

    Readline::readline("Press ENTER to exit")
end 
