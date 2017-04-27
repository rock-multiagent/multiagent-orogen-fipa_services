#! /usr/bin/env ruby

require 'pocolog'
require 'fipa-message'
require 'descriptive_statistics'
require 'erb'

include Pocolog

sender_filename = ARGV[0]

def analyse_file(filename, realtime_offset)
    file = Logfiles.new File.open(filename)
    file.streams.each do |s|
        puts "#{filename} -- available stream: #{s.name}"
    end

    data_stream = file.streams.find { |s| s.name !~ /state/ }
    puts "#{filename} -- reading from stream: #{data_stream.name}"

    data = Hash.new

    idx = 0
    data_stream.samples.each do |realtime, logical, sample|
        letter = sample
        msg = letter.getACLMessage
        size = msg.getContent.size
        # Here we correct for a timeshift, due to timezone etc
        delta = realtime.to_f + realtime_offset - letter.getBaseEnvelope.getDate.to_f
        if !data.has_key?(size)
            data[size] = [ delta ]
        else
            data[size] << delta
        end

        puts "Read sample: #{idx} of #{data_stream.size}\n"
        idx += 1
        #if idx > 10 
        #    break
        #end
    end
    return data
end

# Here we can correct for a timeshift, due to timezone etc
analysis = analyse_file(sender_filename, -8*60*60)
#analysis = analyse_file(sender_filename, 0)

timestamp = Time.now.strftime("%Y%m%d-%H%M%S")
dir = "#{timestamp}_analysis"
FileUtils.mkdir_p dir

filename = File.join(dir, "#{timestamp}_raw.data")
puts "Writing data to file: #{filename}"
File.open(filename,'w') do |file|
    analysis.each do |size, rtt| 
        file.write "#{size} #{rtt}\n"
    end
end

filename = File.join(dir, "#{timestamp}_analysis.data")
puts "Writing data to file: #{filename}"
File.open(filename,'w') do |file|
    file.write "# value mean variance stddev\n"
    analysis.each do |size, rtt|
        file.write "#{size} #{rtt.mean} #{rtt.variance} #{rtt.standard_deviation}\n"
    end
end

template = ERB.new(File.read("templates/communication.gnuplot-template"), nil, "%<>")
rendered = template.result(binding)
outfile = File.join(dir, "#{timestamp}_communication.gnuplot")
File.open(outfile,"w") do |io|
    io.write(rendered)
end
puts "Written gnuplot script: #{outfile} -- trying to execute gnuplot"
system("gnuplot #{outfile}")


