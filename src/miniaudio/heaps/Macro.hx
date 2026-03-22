package miniaudio.heaps;

class Macro
{
	public static macro function main()
	{
		#if heaps
		hxd.res.Config.addExtension('ogg', 'miniaudio.heaps.AudioBuffer');
		#end
		return null;
	}
}
