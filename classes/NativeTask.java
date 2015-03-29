import java.io.IOException;

class NativeTask {
	public NativeTask() {}
	static {
		System.loadLibrary("interrupt");
	}

	/**
	 * Do a long task (well, it doesn't really, of course, this is example code,
	 * but it pretends by sleeping for 25s).  It is an error to call doTask() on
	 * more than one thread at once (ie, the calls must be strictly serialised).
	 */
	public void doTask()
			throws InterruptedException {
		long start = System.currentTimeMillis();
		NativeTaskSelector selector = new NativeTaskSelector(this);
		try {
			selector.registerStart();
			if (doTask0()) {
				System.out.println("Task woken up...");
				if (Thread.interrupted())
					throw new InterruptedException();
			}
		} finally {
			selector.registerEnd();
			try { selector.close(); } catch (IOException impossible) {}
			long elapsed = System.currentTimeMillis() - start;
			System.out.println("doTask() took " + elapsed + "ms");
		}
	}

	/**
	 * Wake up the NativeTask if it is inside doTask(), which will return
	 * immediately.
	 */
	public void wakeupTask() {
		wakeupTask0();
	}

	/**
	 * Native implementation of wakeupTask().
	 */
	native private void wakeupTask0();
	/**
	 * Native implementation of the task.
	 * @return true if the task was interrupted by a call to wakeupTask().
	 */
	native private boolean doTask0();
	private long eventHandle;
}
