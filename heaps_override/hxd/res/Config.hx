package hxd.res;

enum Platform
{
	HL;
	JS;
	Unknown;
}

class Config
{
	public static var extensions = [
		"jpg,png,jpeg,gif,tga,dds,hdr" => "hxd.res.Image",
		"fbx,hmd" => "hxd.res.Model",
		"ttf" => "hxd.res.Font",
		"fnt" => "hxd.res.BitmapFont",
		"bdf" => "hxd.res.BDFFont",
		"wav,mp3,ogg,flac,opus" => "miniaudio.heaps.AudioBuffer",
		"tmx" => "hxd.res.TiledMap",
		"atlas" => "hxd.res.Atlas",
		"grd" => "hxd.res.Gradients",
		#if hide
		"prefab,fx,fx2d,l3d,shgraph" => "hxd.res.Prefab", "world" => "hxd.res.World", "animgraph" => "hxd.res.AnimGraph",
		#end
	];

	public static function addExtension(extension, className)
	{
		extensions.set(extension, className);
	}

	public static var ignoredExtensions = ["gal" => true, "lch" => true, "fla" => true,];

	public static var ignoredDirs:Map<String, Bool> = [];

	public static var pairedExtensions = [
		"fnt" => "png",
		"fbx" => "png,jpg,jpeg,gif,tga",
		"cdb" => "img",
		"atlas" => "png",
		"ogg" => "wav",
		"mp3" => "wav",
		"l3d" => "bake",
		"css" => "less,css.map",
	];

	public static function addPairedExtension(main, shadow)
	{
		if (pairedExtensions.exists(main))
			pairedExtensions.set(main, pairedExtensions.get(main) + "," + shadow);
		else
			pairedExtensions.set(main, shadow);
	}

	static function defined(name:String)
	{
		return switch (name)
		{
			case "js":
				#if js true #else false #end;
			case "hl":
				#if hl true #else false #end;
			case "heaps_enable_hl_mp3":
				#if heaps_enable_hl_mp3 true #else false #end;
			case "stb_ogg_sound":
				#if stb_ogg_sound true #else false #end;
			default:
				false;
		};
	}

	static function init()
	{
		var pf = if (defined("js")) JS else if (defined("hl")) HL else Unknown;
		switch (pf)
		{
			case HL:
				if (!defined("heaps_enable_hl_mp3"))
					ignoredExtensions.set("mp3", true);
			default:
				if (!defined("stb_ogg_sound"))
					ignoredExtensions.set("ogg", true);
		}
		return pf;
	}

	public static var platform:Platform = init();
}
