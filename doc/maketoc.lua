local path = ...

local file = assert(io.open(path))

local function makeanchor(title)
	return title:gsub("%p", ""):gsub("%s", "-"):lower()
end

local function underlined(kind, line, previous)
	local under = line:match("^"..kind:rep(3).."+$")
	if under == line and #under == #previous then
		return previous
	end
end

local previous, sections, entries
for line in file:lines() do
	if line == "^#%s*Contents%s*$"
	or underlined("=", line, previous) == "Contents" then
		sections = {}
	elseif sections ~= nil then
		local name = line:match("^%s*##%s(.-)%s*$")
		          or underlined("%-", line, previous)
		if name ~= nil then
			entries = {
				title = name,
				anchor = makeanchor(name),
				empty = true,
			}
			table.insert(sections, entries)
		elseif entries ~= nil then
			if entries.empty then
				entries.empty = not line:match("%S")
				local module = line:match("^Module `([%w%.:]+)`")
				if module ~= nil then
					entries.title = "`"..module.."`"
				end
			else
				local name, extra = line:match("### `([%w%.:]+)([^`]-)`")
				if name ~= nil then
					table.insert(entries, {
						title = "`"..name.."`",
						anchor = makeanchor(name..extra),
					})
				end
			end
		end
	end
	previous = line
end

file:close()

for _, section in ipairs(sections) do
	io.write("- [", section.title, "](#", section.anchor, ")\n")
	for _, entry in ipairs(section) do
		io.write("\t- [", entry.title, "](#", entry.anchor, ")\n")
	end
end
