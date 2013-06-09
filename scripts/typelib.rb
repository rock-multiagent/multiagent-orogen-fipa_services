# This is a typelib plugin which relies on the FipaMessage library 
# It allows to send fipa letters from ruby to orocos ports of type
# /fipa/SerializedLetter
begin 
    require 'fipa-message'
	Typelib.convert_to_ruby '/fipa/SerializedLetter' do |value|
	  	# from_byte_array is defined in the rice extension of FipaMessage
		FIPA::Utils.deserialize(value.data.to_a)
	end

	Typelib.convert_from_ruby FIPA::ACLEnvelope, '/fipa/SerializedLetter' do |value, typelib_type|
		# create an object of type '/fipa/SerialiedLetter
		result = typelib_type.new

		# assign our array to the vector
		# conversion between vector and array is supported by typelib
		# so we don't have to do anything else here
		result.data = FIPA::Utils.serialize(value)
		result
	end

rescue LoadError
	STDERR.puts "The fipa-message library is not present, I am thus unable to support ruby conversion for fipa messages"
end



