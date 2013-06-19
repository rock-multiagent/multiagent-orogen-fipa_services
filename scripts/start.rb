require 'readline'
require 'orocos'
include Orocos

Orocos.initialize

prefix = ARGV[0]

Orocos.run "mts::Task" => "#{prefix}mts" , :wait => 10, :cmdline_args => { :prefix => prefix }, :valgrind => false do |p1|

begin
    mts_module = TaskContext.get "#{prefix}mts"
rescue Orocos::NotFound
    print 'Deployment not found.'
    raise
end

    mts_module.avahi_type = "_test_mts._tcp"
    mts_module.avahi_port = 12000
    mts_module.avahi_ttl = 0
    mts_module.configure
    mts_module.start

    while true
        sleep 1
        mts_module.trigger
    end
end 
