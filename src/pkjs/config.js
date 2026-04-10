/**
 * "BGColor",
 * "FGColor"
*/
module.exports = [
	{
		"type": "heading",
		"defaultValue": "App Configuration"
	},
	{
		"type": "heading",
		"defaultValue": "Colors"
	},
	{
		"type": "color",
		"messageKey": "FGColor",
		"label": "Foreground color",
		"defaultValue": "0xFFFFFF",
		"allowGray": false,
		"capabilities": ["BW"],
	},
	{
		"type": "color",
		"messageKey": "FGColor",
		"label": "Foreground color",
		"defaultValue": "0xFFFF55",
		"allowGray": false,
		"capabilities": ["COLOR"],
	},
	{
		"type": "color",
		"messageKey": "BGColor",
		"label": "Background color",
		"defaultValue": "0x000000",
		"allowGray": false
	},
	{
		"type": "slider",
		"messageKey": "ActivationTime",
		"label": "Simulation Duration (ms)",
		"defaultValue": "10000",
		"min": 500,
		"max": 30000,
		"step": 500,
	},
	{
		"type": "submit",
		"defaultValue": "Save Settings"
	}
];
