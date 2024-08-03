local repeats<const> = 100
local histlen<const> = 8
local buffer<const> = memory.create(8192)

local histogram = setmetatable({}, {__index = function () return 0 end})
for i = 1, 100 do
	system.random(buffer)
	for j = 1, #buffer do
		local pos = 1 + (buffer:get(j) % histlen)
		histogram[pos] = histogram[pos] + 1
	end
end
print(table.unpack(histogram, 1, histlen))
