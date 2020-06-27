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
local lineno, links = 0, {}
for line in file:lines() do
	lineno = lineno+1
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
		else
			local name, url = line:match("%[([^%]]+)%]%(([^%)]+)%)")
			if name ~= nil then
				local anchor = url:match("^#([%w%-]+)$")
				if anchor ~= nil then
					table.insert(links, { anchor = anchor, line = lineno })
				end
			end
			if entries ~= nil then
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
	end
	previous = line
end

file:close()

local function writeanchor(prefix, title, anchor)
	local previous = links[anchor]
	if previous ~= nil then
		io.stderr:write("WARN : ", title, " has the same anchor of ", previous, "\n")
	else
		links[anchor] = title
	end
	io.write(prefix, "- [", title, "](#", anchor, ")\n")
end

for _, section in ipairs(sections) do
	writeanchor("", section.title, section.anchor)
	for _, entry in ipairs(section) do
		writeanchor("\t", entry.title, entry.anchor)
	end
end

for _, link in ipairs(links) do
	if links[link.anchor] == nil then
		io.stderr:write("ERROR: invalid anchor ", link.anchor, " on line ", link.line, "\n")
	end
end