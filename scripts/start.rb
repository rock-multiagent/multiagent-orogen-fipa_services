require 'fipa-message'
include FIPA

require 'orocos'
include Orocos

Orocos.initialize

deployment_name = ENV['FAMOS_SYSTEM_ID']+'_'+'ROOT01'
mta_name = ENV['FAMOS_SYSTEM_ID']+'_'+'MTA'

Orocos.run deployment_name, "wait" => 100 do |p1|

begin
    root_module = p1.task deployment_name
rescue Orocos::NotFound
    print 'Deployment not found.'
    raise
end
    # deprecated: has no effect, use dedicate deployments instead
    #root_module.module_name = 'A_ROOT_1'

    root_module.avahi_type = '_rimres._tcp'
    root_module.avahi_port = 12000
    root_module.avahi_ttl = 0
    root_module.configure
    root_module.start

    while true
        sleep 1
    end
end 
