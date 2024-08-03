local histogram<const> = { 0, 0, 0, 0, 0, 0, 0, 0 }
local buffer<const> = memory.create(8192)

for i = 1, 100 do
	system.random(buffer)
	for j = 1, #buffer do
		local pos = 1 + (buffer:get(j) % #histogram)
		histogram[pos] = histogram[pos] + 1
	end
	io.write("."); io.flush()
end
print()
print(table.unpack(histogram))
