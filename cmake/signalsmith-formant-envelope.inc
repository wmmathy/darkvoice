			// DarkVoice experimental envelope estimator: average one fundamental spacing.
			// Scratch is allocated by configure; processing is O(bands), no allocation.
			std::copy_n(formantMetric.begin(), bands, formantSource.begin());
			int radius = std::clamp(static_cast<int>(std::ceil(freqEstimate*Sample(0.5))), 1, std::max(1, bands - 1));
			int width = 2*radius + 1;
			double sum = 0, correction = 0;
			auto add = [&](double value) {
				double next = sum + value;
				correction += std::abs(sum) >= std::abs(value) ? (sum - next) + value : (value - next) + sum;
				sum = next;
			};
			int low = 0, high = -1;
			for (int b = 0; b < bands; ++b) {
				int nextLow = std::max(0, b - radius), nextHigh = std::min(bands - 1, b + radius);
				if (b % width == 0) {
					sum = correction = 0;
					low = nextLow;
					high = nextHigh;
					for (int i = low; i <= high; ++i) add(formantSource[i]);
				} else {
					while (low < nextLow) add(-static_cast<double>(formantSource[low++]));
					while (high < nextHigh) add(formantSource[++high]);
				}
				formantMetric[b] = static_cast<Sample>(std::max(0.0, sum + correction)/(high - low + 1));
			}
