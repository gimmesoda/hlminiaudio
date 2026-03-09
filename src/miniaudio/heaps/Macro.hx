package miniaudio.heaps;

class Macro
{
	public static macro function main()
	{
		#if heaps
		hxd.res.Config.addExtension('ogg', 'miniaudio.heaps.AudioBuffer');
		#if hlopus
		hxd.res.Config.addExtension('opus', 'miniaudio.heaps.AudioBuffer');
		#end
		#end
		return null;
	}
}
